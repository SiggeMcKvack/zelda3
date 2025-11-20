// Zelda3 GTK3 Launcher
// Entry point and main initialization

#include <gtk/gtk.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "launcher_ui.h"
#include "config_writer.h"
#include "config_reader.h"
#include "config.h"
#include "logging.h"

// Global state
static Config g_launcher_config;
static GtkWidget *g_main_window = NULL;
static char kConfigPath[1024] = "./zelda3.ini";  // Will be initialized to absolute path in main()

// Check if file exists
static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

// Load or create config
static bool load_or_create_config(Config *config) {
    if (file_exists(kConfigPath)) {
        LogInfo("Loading existing config from %s", kConfigPath);
        if (!ConfigReader_Read(kConfigPath, config)) {
            LogWarn("Failed to parse config, using defaults");
            ConfigWriter_InitDefaults(config);
        } else {
            LogInfo("Config loaded successfully - fullscreen=%d, window_width=%d, features0=0x%x",
                    config->fullscreen, config->window_width, config->features0);
        }
        return true;
    } else {
        LogInfo("No config found, creating defaults");
        ConfigWriter_InitDefaults(config);

        // Write defaults to file
        if (!ConfigWriter_Write(kConfigPath, config)) {
            LogError("Failed to write default config");
            return false;
        }

#ifndef NDEBUG
        printf("✓ Created default config at %s\n", kConfigPath);
#endif
        return true;
    }
}

// Save config and optionally launch game
static bool save_config(void) {
    LogInfo("Saving configuration...");

    // Update config from UI
    LauncherUI_UpdateConfigFromUI(&g_launcher_config);

    LogInfo("Config to save - fullscreen=%d, window_width=%d, features0=0x%x",
            g_launcher_config.fullscreen, g_launcher_config.window_width, g_launcher_config.features0);

    char error_buf[256];
    if (!ConfigWriter_Validate(&g_launcher_config, error_buf, sizeof(error_buf))) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(g_main_window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Config validation failed: %s",
            error_buf
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return false;
    }

    if (!ConfigWriter_Write(kConfigPath, &g_launcher_config)) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(g_main_window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Failed to write config to %s",
            kConfigPath
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return false;
    }

    LogInfo("Config saved successfully");
    return true;
}

// Get directory containing the launcher executable
static void get_executable_dir(char *buf, size_t buf_size) {
    #ifdef __APPLE__
        uint32_t size = buf_size;
        if (_NSGetExecutablePath(buf, &size) == 0) {
            // Get directory part
            char *last_slash = strrchr(buf, '/');
            if (last_slash) *last_slash = '\0';
        } else {
            strcpy(buf, ".");
        }
    #elif defined(__linux__)
        ssize_t len = readlink("/proc/self/exe", buf, buf_size - 1);
        if (len != -1) {
            buf[len] = '\0';
            char *last_slash = strrchr(buf, '/');
            if (last_slash) *last_slash = '\0';
        } else {
            strcpy(buf, ".");
        }
    #else
        strcpy(buf, ".");
    #endif
}

// Launch the zelda3 game executable
static bool launch_game(void) {
    LogInfo("Launching game...");

    // Find zelda3 executable in same directory as launcher
    char launcher_dir[512];
    get_executable_dir(launcher_dir, sizeof(launcher_dir));

    char game_exe[1024];
    snprintf(game_exe, sizeof(game_exe), "%s/zelda3", launcher_dir);

    if (!file_exists(game_exe)) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(g_main_window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Game executable not found: %s\n\n"
            "Make sure zelda3 is in the same directory as the launcher.",
            game_exe
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return false;
    }

    // Launch game in background
    #ifdef _WIN32
        // Windows: Use system() with START command
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "START \"\" \"%s\"", game_exe);
        system(cmd);
    #else
        // Unix: Fork and exec
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: execute game
            execl(game_exe, "zelda3", NULL);
            // If execl returns, it failed
            LogError("Failed to execute game: %s", game_exe);
            exit(1);
        } else if (pid < 0) {
            // Fork failed
            LogError("Failed to fork process");
            return false;
        }
        // Parent continues
    #endif

    LogInfo("Game launched successfully");
    return true;
}

// Button callback: Apply (save config without closing)
static void on_save_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;

    if (save_config()) {
        LogInfo("Configuration saved successfully");

        // Show confirmation dialog
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(g_main_window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "Settings saved to %s",
            kConfigPath
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

// Button callback: Apply & Launch (save, launch game, and close)
static void on_save_and_launch_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;

    if (save_config()) {
        if (launch_game()) {
            gtk_main_quit();
        }
    }
}

// Button callback: Close without saving
static void on_cancel_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;

    LogInfo("Closed without saving");
    gtk_main_quit();
}

// Create full launcher window with tabs
static GtkWidget* create_launcher_window(void) {
    GtkWidget *window = LauncherUI_CreateWindow(&g_launcher_config);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Add button box to bottom of window
    GtkWidget *vbox = gtk_bin_get_child(GTK_BIN(window));
    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(button_box), 5);
    gtk_container_set_border_width(GTK_CONTAINER(button_box), 5);
    gtk_box_pack_end(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);

    // Close button
    GtkWidget *cancel_btn = gtk_button_new_with_label("Close");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_clicked), NULL);
    gtk_container_add(GTK_CONTAINER(button_box), cancel_btn);

    // Apply button (saves without closing)
    GtkWidget *save_btn = gtk_button_new_with_label("Apply");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_clicked), NULL);
    gtk_container_add(GTK_CONTAINER(button_box), save_btn);

    // Apply & Launch button
    GtkWidget *launch_btn = gtk_button_new_with_label("Apply & Launch");
    g_signal_connect(launch_btn, "clicked", G_CALLBACK(on_save_and_launch_clicked), NULL);
    gtk_container_add(GTK_CONTAINER(button_box), launch_btn);

    return window;
}

int main(int argc, char *argv[]) {
#ifdef NDEBUG
    // Release build: Suppress all console output on macOS
    // This prevents Terminal window from staying open
    #ifdef __APPLE__
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    #endif
#else
    printf("Zelda3 GTK3 Launcher\n");
    printf("====================\n\n");
#endif

    // Initialize logging
    InitializeLogging();

    // Set config path to executable directory
    char exe_dir[1024];
    get_executable_dir(exe_dir, sizeof(exe_dir));
    snprintf(kConfigPath, sizeof(kConfigPath), "%s/zelda3.ini", exe_dir);
    LogInfo("Config path: %s", kConfigPath);

    // Suppress GTK icon warnings on macOS
    #ifdef __APPLE__
        g_log_set_handler("Gtk", G_LOG_LEVEL_WARNING,
                         (GLogFunc)gtk_false, NULL);
    #endif

    // Initialize GTK
    if (!gtk_init_check(&argc, &argv)) {
        LogError("Failed to initialize GTK");
        return 1;
    }

    LogInfo("GTK initialized (version: %d.%d.%d)",
            gtk_get_major_version(),
            gtk_get_minor_version(),
            gtk_get_micro_version());

    // Initialize SDL2 for gamepad support
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
        LogError("Failed to initialize SDL2: %s", SDL_GetError());
        return 1;
    }
    LogInfo("SDL2 initialized for gamepad support");

    // Load or create config
    if (!load_or_create_config(&g_launcher_config)) {
        LogError("Failed to load/create config");
        SDL_Quit();
        return 1;
    }

    // Create launcher window
    g_main_window = create_launcher_window();
    gtk_widget_show_all(g_main_window);

    // Bring window to front
    gtk_window_present(GTK_WINDOW(g_main_window));

    // macOS: Force app activation using native API
    #ifdef __APPLE__
    extern void ActivateMacOSApp(void);
    ActivateMacOSApp();
    #endif

    LogInfo("Starting GTK main loop");

    // Run GTK main loop
    gtk_main();

    // Cleanup
    LogInfo("Shutting down launcher");
    SDL_Quit();

    return 0;
}
