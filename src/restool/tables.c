// tables.c - Lookup tables for YAML asset extraction
// AUTO-GENERATED from assets/tables.py by generate_tables.py
// DO NOT EDIT MANUALLY
#include "tables.h"
#include <string.h>

// ============================================================================
// Object Type Names
// ============================================================================

const char *kType0Names[kType0NamesCount] = {
  "00-Ceiling [L-R]",
  "01-[N]Wall Horz: [L-R]",
  "02-[S]Wall Horz: [L-R]",
  "03-[N]Wall Horz: (LOW) [L-R]",
  "04-[S]Wall Horz: (LOW) [L-R]",
  "05-[N]Wall Column [L-R]",
  "06-[S]Wall Column [L-R]",
  "07-[N]Wall Pit [L-R]",
  "08-[S]Wall Pit [L-R]",
  "09-/ Wall Wood Bot (HIGH) [NW]",
  "0A-\\ Wall Wood Bot (HIGH) [SW]",
  "0B-\\ Wall Wood Bot (HIGH) [NE]",
  "0C-/ Wall Wood Bot (HIGH) [SE]",
  "0D-/ Wall Tile Bot (HIGH) [NW]",
  "0E-\\ Wall Tile Bot (HIGH) [SW]",
  "0F-\\ Wall Tile Bot (HIGH) [NE]",
  "10-/ Wall Tile Bot (HIGH) [SE]",
  "11-/ Wall Tile2 Bot (HIGH) [NW]",
  "12-\\ Wall Tile2 Bot (HIGH) [SW]",
  "13-\\ Wall Tile2 Bot (HIGH) [NE]",
  "14-/ Wall Tile2 Bot (HIGH) [SE]",
  "15-/ Wall Tile Top (LOW)[NW]",
  "16-\\ Wall Tile Top (LOW)[SW]",
  "17-\\ Wall Tile Top (LOW)[NE]",
  "18-/ Wall Tile Top (LOW)[SE]",
  "19-/ Wall Tile Bot (LOW)[NW]",
  "1A-\\ Wall Tile Bot (LOW)[SW]",
  "1B-\\ Wall Tile Bot (LOW)[NE]",
  "1C-/ Wall Tile Bot (LOW)[SE]",
  "1D-/ Wall Tile2 Bot (LOW)[NW]",
  "1E-\\ Wall Tile2 Bot (LOW)[SW]",
  "1F-\\ Wall Tile2 Bot (LOW)[NE]",
  "20-/ Wall Tile2 Bot (LOW)[SE]",
  "21-Mini Stairs [L-R]",
  "22-Horz: Rail Thin [L-R]",
  "23-Pit [N]Edge [L-R]",
  "24-Pit [N]Edge [L-R]",
  "25-Pit [N]Edge [L-R]",
  "26-Pit [N]Edge [L-R]",
  "27-Pit [N]Edge [L-R]",
  "28-Pit [S]Edge [L-R]",
  "29-Pit [S]Edge [L-R]",
  "2A-Pit [N]Edge [L-R]",
  "2B-Pit [SE]Corner [L-R]",
  "2C-Pit [SW]Corner [L-R]",
  "2D-Pit [NE]Corner [L-R]",
  "2E-Pit [NW]Corner [L-R]",
  "2F-Rail Wall [L-R]",
  "30-Rail Wall [L-R]",
  "31-Unused -empty",
  "32-Unused -empty",
  "33-Red Carpet Floor [L-R]",
  "34-Red Carpet Floor Trim [L-R]",
  "35-Unused -empty",
  "36-[N]Curtain [L-R]",
  "37-[W]Curtain [L-R]-unused-",
  "38-Statue [L-R]",
  "39-Column [L-R]",
  "3A-[N]Wall Decor: [L-R]",
  "3B-[S]Wall Decor: [L-R]",
  "3C-Double Chair [L-R]",
  "3D-Stand Torch [L-R]",
  "3E-[N]Wall Column [L-R]",
  "3F-Water Edge [L-R]",
  "40-Water Edge [L-R]",
  "41-Water Edge [L-R]",
  "42-Water Edge [L-R]",
  "43-Water Edge [L-R]",
  "44-Water Edge [L-R]",
  "45-Water Edge [L-R]",
  "46-Water Edge [L-R]",
  "47-Unused Waterfall [L-R]",
  "48-Unused Waterfall [L-R]",
  "49-N/A",
  "4A-N/A",
  "4B-[S]Wall Column [L-R]",
  "4C-Bar [L-R]",
  "4D-Shelf [L-R]",
  "4E-Shelf [L-R]",
  "4F-Shelf [L-R]",
  "50-Cane Ride [L-R]",
  "51-[N]Canon Hole [L-R]",
  "52-[S]Canon Hole [L-R]",
  "53-Cane Ride [L-R]",
  "54-Unused [L-R]",
  "55-[N]Wall Torches [L-R]",
  "56-[S]Wall Torches [L-R]",
  "57-Unused",
  "58-Unused",
  "59-Unused",
  "5A-Unused",
  "5B-[N]Canon Hole [L-R]",
  "5C-[S]Canon Hole [L-R]",
  "5D-Large Horz: Rail [L-R]",
  "5E-Block [L-R]",
  "5F-Long Horz: Rail [L-R]",
  "60-Ceiling [U-D]",
  "61-[W]Wall Vert: [U-D]",
  "62-[E]Wall Vert: [U-D]",
  "63-[W]Wall Vert: (LOW) [U-D]",
  "64-[E]Wall Vert: (LOW) [U-D]",
  "65-[W]Wall Column [U-D]",
  "66-[E]Wall Column [U-D]",
  "67-[W]Wall Pit [U-D]",
  "68-[E]Wall Pit [U-D]",
  "69-Vert: Rail Thin [U-D]",
  "6A-[W]Pit Edge [U-D]",
  "6B-[E]Pit Edge [U-D]",
  "6C-[W]Rail Wall [U-D]",
  "6D-[E]Rail Wall [U-D]",
  "6E-Unused",
  "6F-Unused",
  "70-Red Floor/Wire Floor [U-D]",
  "71-Red Carpet Floor Trim [U-D]",
  "72-Unused",
  "73-[W]Curtain [U-D]",
  "74-[E]Curtain [U-D]",
  "75-Column [U-D]",
  "76-[W]Wall Decor: [U-D]",
  "77-[E]Wall Decor: [U-D]",
  "78-[W]Wall Top Column [U-D]",
  "79-Water Edge [U-D]",
  "7A-Water Edge [U-D]",
  "7B-[E]Wall Top Column [U-D]",
  "7C-Cane Ride [U-D]",
  "7D-Pipe Ride [U-D]",
  "7E-Unused",
  "7F-[W]Wall Torches [U-D]",
  "80-[E]Wall Torches [U-D]",
  "81-[W]Wall Decor: [U-D]",
  "82-[E]Wall Decor: [U-D]",
  "83-[W]Wall Decor:?? [U-D]",
  "84-[E]Wall Decor:?? [U-D]",
  "85-[W]Wall Canon Hole [U-D]",
  "86-[E]Wall Canon Hole [U-D]",
  "87-Floor Torch [U-D]",
  "88-Large Vert: Rail [U-D]",
  "89-Block Vert: [U-D]",
  "8A-Long Vert: Rail [U-D]",
  "8B-[W]Vert: Jump Edge [U-D]",
  "8C-[E]Vert: Jump Edge [U-D]",
  "8D-[W]Edge [U-D]",
  "8E-[E]Edge [U-D]",
  "8F-N/A",
  "90-[W]Wall Vert: [U-D]",
  "91-[E]Wall Horz: [U-D]",
  "92-Blue Peg Block [U-D]",
  "93-Orange Peg Block [U-D]",
  "94-Invisible Floor [U-D]",
  "95-Fake Pot [U-D]",
  "96-Hammer Peg Block [U-D]",
  "97-Unused",
  "98-Unused",
  "99-Unused",
  "9A-Unused",
  "9B-Unused",
  "9C-Unused",
  "9D-Unused",
  "9E-Unused",
  "9F-Unused",
  "A0-/ Ceiling [NW]",
  "A1-\\ Ceiling [SW]",
  "A2-\\ Ceiling [NE]",
  "A3-/ Ceiling [SE]",
  "A4-Hole [4-way]",
  "A5-/ Ceiling [Trans][NW]",
  "A6-\\ Ceiling [Trans][SW]",
  "A7-\\ Ceiling [Trans][NE]",
  "A8-/ Ceiling [Trans][SE]",
  "A9-/ Ceiling [BG2 X-RAY][SE]",
  "AA-\\ Ceiling [BG2 X-RAY][NE]",
  "AB-\\ Ceiling [BG2 X-RAY][SW]",
  "AC-/ Ceiling [BG2 X-RAY][NW]",
  "AD-N/A",
  "AE-N/A",
  "AF-N/A",
  "B0-[S]Horz: Jump Edge [L-R]",
  "B1-[S]Horz: Jump Edge [L-R]",
  "B2-Floor? [L-R]",
  "B3-N/A",
  "B4-N/A",
  "B5-N/A",
  "B6-[N]Wall Decor: 1/2 [L-R]",
  "B7-[S]Wall Decor: 1/2 [L-R]",
  "B8-Blue Switch Block [L-R]",
  "B9-Red Switch Block [L-R]",
  "BA-Invisible Floor [L-R]",
  "BB-N/A",
  "BC-fake pots [L-R]",
  "BD-Hammer Pegs [L-R]",
  "BE-Unused",
  "BF-Unused",
  "C0-Ceiling Large [4-way]",
  "C1-Chest Pedastal [4-way]",
  "C2-Falling Edge Mask [4-way]",
  "C3-Falling Edge Mask [4-way]",
  "C4-Doorless Room Transition",
  "C5-Floor3 [4-way]",
  "C6-BG2 X-RAY Overlay [4-way]",
  "C7-Floor4 [4-way]",
  "C8-Water Floor [4-way]",
  "C9-Water Floor2 [4-way]",
  "CA-Floor5 [4-way]",
  "CB-Unused",
  "CC-Unused",
  "CD-Moving Wall Right [4-way]",
  "CE-Moving Wall Left [4-way]",
  "CF-Unused",
  "D0-Unused",
  "D1-Water Floor3 [4-way]",
  "D2-Floor6 [4-way]",
  "D3-Unused",
  "D4-Unused",
  "D5-Unused",
  "D6-N/A",
  "D7-overlay tile? [4-way]",
  "D8-Lava Background? [4-way]",
  "D9-Swimming Overlay [4-way]",
  "DA-Lava Background 2 [4-way]",
  "DB-Floor2 [4-way]",
  "DC-Chest Platform? [4-way]",
  "DD-Table / Rock [4-way]",
  "DE-Spike Block [4-way]",
  "DF-Spike Floor [4-way]",
  "E0-Floor7 [4-way]",
  "E1-Floor9 [4-way]",
  "E2-Rupee Floor [4-way]",
  "E3-Moving Floor Up [4-way]",
  "E4-Moving Floor Down [4-way]",
  "E5-Moving Floor Left [4-way]",
  "E6-Moving Floor Right [4-way]",
  "E7-Moving Floor/Water [4-way]",
  "E8-Weird Floor? [4-way]",
  "E9-Unused",
  "EA-Unused",
  "EB-Unused",
  "EC-Unused",
  "ED-Unused",
  "EE-Unused",
  "EF-Unused",
  "F0-Unused",
  "F1-Unused",
  "F2-Unused",
  "F3-Unused",
  "F4-Unused",
  "F5-Unused",
  "F6-Unused",
  "F7-Unused"
};

const char *kType1Names[kType1NamesCount] = {
  "F80-Water Face",
  "F81-Waterfall Face",
  "F82-Waterfall Face Longer",
  "F83-Cane Ride Spawn [?]Block",
  "F84-Cane Ride Node [4-way]",
  "F85-Cane Ride Node [S-E]",
  "F86-Cane Ride Node [N-E]",
  "F87-Cane Ride Node [S-E]-2",
  "F88-Cane Ride Node [N-E]-2",
  "F89-Cane Ride Node [W-S-E]",
  "F8A-Cane Ride Node [W-N-E]",
  "F8B-Cane Ride Node [N-E-S]",
  "F8C-Cane Ride Node [N-W-S]",
  "F8D-Prison Cell",
  "F8E-Cane Ride Spawn [?]Block",
  "F8F-?",
  "F90-?",
  "F91-?",
  "F92-Rupee Floor",
  "F93-Telepathic Tile",
  "F94-Down Warp Door",
  "F95-Kholdstare Shell - BG2",
  "F96-Single Hammer Peg",
  "F97-Cell",
  "F98-Cell Lock",
  "F99-Chest",
  "F9A-Open Chest",
  "F9B-Stair",
  "F9C-Stair [S](Layer)",
  "F9D-Stair Wet [S](Layer)",
  "F9E-Staircase going Up(Up)",
  "F9F-Staircase Going Down (Up)",
  "FA0-Staircase Going Up (Down)",
  "FA1-Staircase Going Down (Down)",
  "FA2-Pit Wall Corner",
  "FA3-Pit Wall Corner",
  "FA4-Pit Wall Corner",
  "FA5-Pit Wall Corner",
  "FA6-Staircase Going Up (Lower)",
  "FA7-Staircase Going Up (Lower)",
  "FA8-Staircase Going Down (Lower)",
  "FA9-Staircase Going Down (Lower)",
  "FAA-Dark Room BG2 Mask",
  "FAB-Staircase Going Down (Lower)",
  "FAC-Large Pick Up Block",
  "FAD-Agahnim Altar",
  "FAE-Agahnim Room",
  "FAF-Pot",
  "FB0-??",
  "FB1-Big Chest",
  "FB2-Big Chest Open",
  "FB3-Stairs Submerged [S](layer)",
  "FB4-???",
  "FB5-???",
  "FB6-???",
  "FB7-???",
  "FB8-???",
  "FB9-???",
  "FBA-Pipe Ride Mouth [S]",
  "FBB-Pipe Ride Mouth [N]",
  "FBC-Pipe Ride Mouth [E]",
  "FBD-Pipe Ride Mouth [W]",
  "FBE-Pipe Ride Corner [S-E]",
  "FBF-Pipe Ride Corner [N-E]",
  "FC0-Pipe Ride Corner [S-W]",
  "FC1-Pipe Ride Corner [N-W]",
  "FC2-Pipe Ride Tunnel [N]",
  "FC3-Pipe Ride Tunnel [S]",
  "FC4-Pipe Ride Tunnel [W]",
  "FC5-Pipe Ride Tunnel [E]",
  "FC6-Pipe Ride Over Mask [U-D]",
  "FC7-Bomb Floor",
  "FC8-Fake Bomb Floor",
  "FC9-Fake Bomb Floor",
  "FCA-Warp Tile",
  "FCB-???",
  "FCC-???",
  "FCD-???",
  "FCE-???",
  "FCF-Inactive Warp",
  "FD0-Floor Switch",
  "FD1-Skull Pot",
  "FD2-Single Blue Peg",
  "FD3-Single Red Peg",
  "FD4-",
  "FD5-???",
  "FD6-Bar Corner [NW]",
  "FD7-Bar Corner [SW]",
  "FD8-Bar Corner [NE]",
  "FD9-Bar Corner [SE]",
  "FDA-Plate on Table",
  "FDB-Water Troof",
  "FDC-Bookshelf",
  "FDD-Forge",
  "FDE-???",
  "FDF-Bottles on Bar",
  "FE0-???",
  "FE1-Left Warp Door",
  "FE2-Right Warp Door",
  "FE3-Fake Floor Switch",
  "FE4-Fireball Shooter",
  "FE5-Medusa Head",
  "FE6-Hole",
  "FE7-Top Crack Wall",
  "FE8-Bottom Crack Wall",
  "FE9-Left Crack Wall",
  "FEA-Right Crack Wall",
  "FEB-Throne/Decor: Object",
  "FEC-???",
  "FED-???",
  "FEE-???",
  "FEF-???",
  "FF0-Window Light",
  "FF1-Floor Light Blind BG2",
  "FF2-Boss Goo/Shell BG2",
  "FF3-Bg2 Full Mask",
  "FF4-Boss Entrance",
  "FF5-Minigame Chest",
  "FF6-???",
  "FF7-???",
  "FF8-???",
  "FF9-???",
  "FFA-???",
  "FFB-Vitreous Boss?",
  "FFC-???",
  "FFD-???",
  "FFE-???",
  "FFF-???"
};

const char *kType2Names[kType2NamesCount] = {
  "100-Wall Outer Corner (HIGH) [NW]",
  "101-Wall Outer Corner (HIGH) [SW]",
  "102-Wall Outer Corner (HIGH) [NE]",
  "103-Wall Outer Corner (HIGH) [SE]",
  "104-Wall Inner Corner (HIGH) [NW]",
  "105-Wall Inner Corner (HIGH) [SW]",
  "106-Wall Inner Corner (HIGH) [NE]",
  "107-Wall Inner Corner (HIGH) [SE]",
  "108-Wall Outer Corner (LOW) [NW]",
  "109-Wall Outer Corner (LOW) [SW]",
  "10A-Wall Outer Corner (LOW) [NE]",
  "10B-Wall Outer Corner (LOW) [SE]",
  "10C-Wall Inner Corner (LOW) [NW]",
  "10D-Wall Inner Corner (LOW) [SW]",
  "10E-Wall Inner Corner (LOW) [NE]",
  "10F-Wall Inner Corner (LOW) [SE]",
  "110-Wall S-Bend (LOW) [N1]",
  "111-Wall S-Bend (LOW) [S1]",
  "112-Wall S-Bend (LOW) [N2]",
  "113-Wall S-Bend (LOW) [S2]",
  "114-Wall S-Bend (LOW) [W1]",
  "115-Wall S-Bend (LOW) [W2]",
  "116-Wall S-Bend (LOW) [E1]",
  "117-Wall S-Bend (LOW) [E2]",
  "118-Wall Pit Corner (Lower) [NW]",
  "119-Wall Pit Corner (Lower) [SW]",
  "11A-Wall Pit Corner (Lower) [NE]",
  "11B-Wall Pit Corner (Lower) [SE]",
  "11C-Fairy Pot",
  "11D-Statue",
  "11E-Star Tile Off",
  "11F-Star Tile On",
  "120-Torch Lit",
  "121-Barrel",
  "122-Weird Bed",
  "123-Table",
  "124-Decoration",
  "125-???",
  "126-???",
  "127-Chair",
  "128-Bed",
  "129-Decoration",
  "12A-Wall Painting",
  "12B-???",
  "12C-???",
  "12D-Floor Stairs Up (room)",
  "12E-Floor Stairs Down (room)",
  "12F-Floor Stairs Down2 (room)",
  "130-Stairs [N](unused)",
  "131-Stairs [N](layer)",
  "132-Stairs [N](layer)",
  "133-Stairs Submerged [N](layer)",
  "134-Block",
  "135-Water Ladder",
  "136-Water Ladder",
  "137-Water Gate Large",
  "138-Door Staircase Up R",
  "139-Door Staircase Down L",
  "13A-Door Staircase Up R (Lower)",
  "13B-Door Staircase Down L (Lower)",
  "13C-Sanctuary Wall",
  "13D-???",
  "13E-Church Pew",
  "13F-???",
  "140-Ceiling [L-R]"
};

// ============================================================================
// Sprite Names
// ============================================================================

const char *kSpriteNames[kSpriteNamesCount] = {
  "00-Raven",
  "01-Vulture",
  "02",
  "03-BigCanon",
  "04-PullSwitch",
  "05-DnSwitch",
  "06-TrapSwitch",
  "07-FloorMove",
  "08-Octorok",
  "09-Mouldrum",
  "0A-4WayOctorok",
  "0B-Chicken",
  "0C-HoveringRock",
  "0D-Cucumber",
  "0E-SnapDragon",
  "0F-OctoBlimp",
  "10",
  "11-Hinox",
  "12-PigSpearMan",
  "13-MiniHelmasaur",
  "14-GargoyleGrate",
  "15-Bubble",
  "16-Mutant",
  "17-BushCrab",
  "18-Moldorm",
  "19-Poe/Ghini",
  "1A-BlackSmith(Frog",
  "1B-AnArrow",
  "1C-Statue",
  "1D-UselessSprite",
  "1E-PegSwitch",
  "1F-SickBoy",
  "20-BombSlug",
  "21-PushSwitch",
  "22-HoppingBulbPlan",
  "23-RedMiri",
  "24-BlueMiri",
  "25-LiveTree",
  "26-BlueOrb",
  "27-Squirrel",
  "28-PersonRm270",
  "29-Thief",
  "2A-DustGirl",
  "2B-TentMan",
  "2C-Lumberjacks",
  "2D",
  "2E-FluteBoy",
  "2F-Person",
  "30-Person",
  "31-FortuneTeller",
  "32-AngryBrother",
  "33-PullForRupees",
  "34-ScaredGirl2",
  "35-HedgeMan",
  "36-Witch",
  "37-Waterfall",
  "38-ArrowTarget",
  "39-GuyByTheSign",
  "3A-Person11_227",
  "3B-DashItem",
  "3C-FarmBoy",
  "3D-ScaredGirl1",
  "3E-RockCrab",
  "3F-PalaceGuard",
  "40-ElectricBarrier",
  "41-BlueSoldier",
  "42-GreenSoldier",
  "43-RedSpearSoldier",
  "44-Warrior",
  "45-HogSpearMan",
  "46-BlueArcher",
  "47-GreenGrassArche",
  "48-RedSpearKnight",
  "49-RedGrassSpearSo",
  "4A-RedBombKnight",
  "4B-Knight",
  "4C-Geldman",
  "4D-Bunny",
  "4E-Tentacle2",
  "4F-Tentacle",
  "50-GlassSquirrel",
  "51-Armos",
  "52-ZoraKing",
  "53-ArmosKnight",
  "54-Lanmolas",
  "55-FireBallZora",
  "56-WalkingZora",
  "57-HyliaObstacle",
  "58-Crab",
  "59-Animal",
  "5A-Animal",
  "5B-WallBubble(L-R)",
  "5C-WallBubble(R-L)",
  "5D-Roller_1",
  "5E-Roller_2",
  "5F-Roller_3",
  "60-Roller_4",
  "61-Beamos",
  "62-MasterSwd",
  "63-SandCrab1",
  "64-SandCrab2",
  "65-ArcherGame",
  "66-Cannon(Right)",
  "67-Cannon(Left)",
  "68-Cannon(Down)",
  "69-Cannon(Up)",
  "6A-MorningStar",
  "6B-CannonSoldier",
  "6C-Teleport",
  "6D-Rat",
  "6E-Rope",
  "6F-Keese",
  "70",
  "71-Leever",
  "72-Pond",
  "73-Priest/Uncle",
  "74-Runner",
  "75-BottleMan",
  "76-Zelda",
  "77-WierdBuble",
  "78-OldWoman",
  "79-Bee",
  "7A-Agahnim",
  "7B-OneShotMagicBal",
  "7C-StalfosHead",
  "7D-BigSpikeBlock",
  "7E-FireBlade",
  "7F-FireBlade2",
  "80-Lanmola",
  "81-WaterBug",
  "82-4Bubbles",
  "83-GreenRocklops",
  "84-RedRocklops",
  "85-BigSpikeBlock",
  "86-Triceritops",
  "87-FireKeese",
  "88-Mothula",
  "89",
  "8A-SpikeBlock",
  "8B-Gibdo",
  "8C-Arrghus",
  "8D-ArrghusFuzz",
  "8E-Shell",
  "8F-Blob",
  "90-WallMaster",
  "91-StalfosKnight",
  "92-Helmasaur",
  "93-RedOrb",
  "94",
  "95-EyeLaser(Right)",
  "96-EyeLaser(Left)",
  "97-EyeLaser(Down)",
  "98-EyeLaser(Up)",
  "99-Penguin",
  "9A-Splash",
  "9B-Wizzrobe",
  "9C",
  "9D-VRat",
  "9E-Ostrich",
  "9F-Rabbit",
  "A0-Uglybird",
  "A1-IceMan",
  "A2-KholdStare",
  "A3",
  "A4",
  "A5-GreenLizard",
  "A6-RedLizard",
  "A7-Stalfos",
  "A8-GreenAirBomber",
  "A9-BlueAirBomber",
  "AA-LikeLike",
  "AB",
  "AC-Apples",
  "AD-OldMan",
  "AE-DownPipe",
  "AF-UpPipe",
  "B0-RightPipe",
  "B1-LeftPipe",
  "B2-Good-Bee",
  "B3-Inscription",
  "B4-BlueChest",
  "B5-BombShop",
  "B6-Kiki",
  "B7-BlindMan",
  "B8",
  "B9-Bully&Whimp(DW)",
  "BA-Whirlpool",
  "BB-ShopMan",
  "BC-OldMan2",
  "BD-Viterous",
  "BE-",
  "BF-Lighting",
  "C0-Item",
  "C1-AgahTalk",
  "C2-RockChip",
  "C3-Half-Bubble",
  "C4-Bully",
  "C5-Shooter",
  "C6-4WayShooter",
  "C7-FuzzyStack",
  "C8-BigFairy",
  "C9-Tektite",
  "CA-Chomp",
  "CB-TriNexx1",
  "CC-TriNexx2",
  "CD-TriNexx3",
  "CE-Blind",
  "CF-SwampSnake",
  "D0-Lynel",
  "D1-Transform/Smoke",
  "D2-Fish",
  "D3-AliveRock",
  "D4-GroundBomb",
  "D5-DiggingGameGuy",
  "D6-Ganon",
  "D7",
  "D8-Heart",
  "D9-Rupee-G",
  "DA-Rupee-B",
  "DB-InTreeRocks",
  "DC-Bomb",
  "DD-4_bombs",
  "DE-8_bombs",
  "DF-Magic",
  "E0-BigMagic",
  "E1-Arrow",
  "E2-10-Arrows",
  "E3-Fairy",
  "E4-Key",
  "E5-Big_Key",
  "E6",
  "E7-Mushroom",
  "E8-FakeSword",
  "E9-ShopMan2",
  "EA-WitchAssistant",
  "EB-HeartPie",
  "EC-PickedObj",
  "ED",
  "EE-Mantle",
  "EF",
  "F0",
  "F1",
  "F2-MedallianTablet",
  "F3-PersonsDoor",
  "F4-FallingRocks",
  "F5",
  "F6",
  "F7",
  "F8",
  "F9",
  "FA",
  "FB",
  "FC",
  "FD",
  "FE",
  "FF",
  "100-CannonRoom",
  "101-01",
  "102-CannonRoom",
  "103-CannonBalls",
  "104-RopeDrp(Snake)",
  "105-StalfosDrop",
  "106-BombDrop",
  "107-MovingFloor",
  "108-Transformer",
  "109-WallMaster",
  "10A-FloorDrop(Sqr)",
  "10B-FloorDrop(Vert)",
  "10C-0C",
  "10D-0D",
  "10E-0E",
  "10F-0F",
  "110-RightEvil",
  "111-LeftEvil",
  "112-DownEvil",
  "113-UpEvil",
  "114-FloorTiles",
  "115-WizzrobeSpawn",
  "116-MiniBats",
  "117-PotTrap",
  "118-StalfosAppear",
  "119-ArmosKnights",
  "11A-BombDrop",
  "11B"
};

// ============================================================================
// Door Tag Names
// ============================================================================

const char *kTagNames[kTagNamesCount] = {
  "None",
  "NW Kill enemy to open",
  "NE Kill enemy to open",
  "SW Kill enemy to open",
  "SE Kill enemy to open",
  "W Kill enemy to open",
  "E Kill enemy to open",
  "N Kill enemy to open",
  "S Kill enemy to open",
  "Clear quadrant to open",
  "Clear room to open",
  "NW Move block to open",
  "NE Move block to open",
  "SW Move block to open",
  "SE Move block to open",
  "W Move block to open",
  "E Move block to open",
  "N Move block to open",
  "S Move block to open",
  "Move block to open",
  "Pull lever to open",
  "Clear level to open door",
  "Switch opens door(Hold)",
  "Switch opens door(Toggle)",
  "Turn off water",
  "Turn on water",
  "Water gate",
  "Water twin",
  "Secret wall (Right)",
  "Secret wall (Left)",
  "Crash",
  "Crash",
  "Use switch to bomb wall",
  "Holes(0)",
  "Open chest for holes(0)",
  "Holes(1)",
  "Holes(2)",
  "Kill enemy to clear level",
  "SE Kill enemy to move block",
  "Trigger activated chest",
  "Use lever to bomb wall",
  "NW Kill enemy for chest",
  "NE Kill enemy for chest",
  "SW Kill enemies for chest",
  "SE Kill enemy for chest",
  "W Kill enemy for chest",
  "E Kill enemy for chest",
  "N Kill enemy for chest",
  "S Kill enemy for chest",
  "Clear quadrant for chest",
  "Clear room for chest",
  "Light torches to open",
  "Holes(3)",
  "Holes(4)",
  "Holes(5)",
  "Holes(6)",
  "Agahnim's room",
  "Holes(7)",
  "Holes(8)",
  "Open chest for holes(8)",
  "Move block to get chest",
  "Kill to open Ganon's door",
  "Light torches to get chest",
  "Kill boss again"
};

// ============================================================================
// Room Property Names
// ============================================================================

const char *kEffectNames[kEffectNamesCount] = {
  "None",
  "01",
  "Moving floor",
  "Moving water",
  "04",
  "Red flashes",
  "Light torch to see floor",
  "Ganon room"
};

const char *kCollisionNames[kCollisionNamesCount] = {
  "One",
  "Both",
  "Both w/scroll",
  "Moving floor",
  "Moving water"
};

const char *kBg2[kBg2Count] = {
  "None",
  "Parallaxing",
  "Dark",
  "On top",
  "Translucent",
  "Parallaxing2",
  "Normal",
  "Addition",
  "Dark room"
};

// ============================================================================
// Audio Names (sparse mappings)
// ============================================================================

const MusicEntry kMusicEntries[kMusicEntriesCount] = {
  {0, "None"},
  {1, "Title"},
  {2, "World_map"},
  {3, "Beginning"},
  {4, "Rabbit"},
  {5, "Forest"},
  {6, "Intro"},
  {7, "Town"},
  {8, "Warp"},
  {9, "Dark_world"},
  {10, "Master_swd"},
  {11, "File_select"},
  {12, "Soldier"},
  {13, "Mountain"},
  {14, "Shop"},
  {15, "Fanfare"},
  {16, "Castle"},
  {17, "Palace"},
  {18, "Cave"},
  {19, "Clear"},
  {20, "Church"},
  {21, "Boss"},
  {22, "Dungeon"},
  {23, "Psychic"},
  {24, "Secret_way"},
  {25, "Rescue"},
  {26, "Crystal"},
  {27, "Fountain"},
  {28, "Pyramid"},
  {29, "Kill_Agah"},
  {30, "Ganon_room"},
  {31, "Last_boss"},
  {32, "Triforce"},
  {33, "Ending"},
  {34, "Staff"},
  {240, "Stop"},
  {241, "Fade_out"},
  {242, "Lower_vol"},
  {243, "Normal_vol"},
  {255, "Same"}
};

const AmbientSoundEntry kAmbientSoundEntries[kAmbientSoundEntriesCount] = {
  {0, "None"},
  {1, "Heavy rain"},
  {3, "Light rain"},
  {5, "Stop"},
  {7, "Earthquake"},
  {9, "Wind"},
  {11, "Flute"},
  {13, "Chime 1"},
  {15, "Chime 2"}
};

// ============================================================================
// Palace/Dungeon Names
// ============================================================================

const char *kPalaceNames[kPalaceNamesCount] = {
  "None",
  "Church",
  "Castle",
  "East",
  "Desert",
  "Agahnim",
  "Water",
  "Dark",
  "Mud",
  "Wood",
  "Ice",
  "Tower",
  "Town",
  "Mountain",
  "Agahnim2"
};

// ============================================================================
// Lookup Functions
// ============================================================================

int FindType0Index(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kType0NamesCount; i++) {
    if (strcmp(kType0Names[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int FindType1Index(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kType1NamesCount; i++) {
    if (strcmp(kType1Names[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int FindType2Index(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kType2NamesCount; i++) {
    if (strcmp(kType2Names[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int FindSpriteIndex(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kSpriteNamesCount; i++) {
    if (strcmp(kSpriteNames[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int FindTagIndex(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kTagNamesCount; i++) {
    if (strcmp(kTagNames[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int FindEffectIndex(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kEffectNamesCount; i++) {
    if (strcmp(kEffectNames[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int FindCollisionIndex(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kCollisionNamesCount; i++) {
    if (strcmp(kCollisionNames[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int FindBg2Index(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kBg2Count; i++) {
    if (strcmp(kBg2[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int FindPalaceIndex(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kPalaceNamesCount; i++) {
    if (strcmp(kPalaceNames[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int FindMusicIndex(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kMusicEntriesCount; i++) {
    if (strcmp(kMusicEntries[i].name, name) == 0) {
      return kMusicEntries[i].index;
    }
  }
  return -1;
}

int FindAmbientSoundIndex(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kAmbientSoundEntriesCount; i++) {
    if (strcmp(kAmbientSoundEntries[i].name, name) == 0) {
      return kAmbientSoundEntries[i].index;
    }
  }
  return -1;
}

const char* GetMusicName(int index) {
  for (int i = 0; i < kMusicEntriesCount; i++) {
    if (kMusicEntries[i].index == index) {
      return kMusicEntries[i].name;
    }
  }
  return NULL;
}

const char* GetAmbientSoundName(int index) {
  for (int i = 0; i < kAmbientSoundEntriesCount; i++) {
    if (kAmbientSoundEntries[i].index == index) {
      return kAmbientSoundEntries[i].name;
    }
  }
  return NULL;
}

// ============================================================================
// Secret Items (Dungeon secrets)
// ============================================================================

const SecretEntry kSecretEntries[] = {
  { 0, "00-Nothing" },
  { 1, "01-Rupee-G" },
  { 2, "02-RockCrab" },
  { 3, "03-Bee" },
  { 4, "04-Random" },
  { 5, "05-Bomb" },
  { 6, "06-Heart" },
  { 7, "07-Rupee-B" },
  { 8, "08-Key" },
  { 9, "09-Arrow" },
  { 10, "0A-Bomb" },
  { 11, "0B-Heart" },
  { 12, "0C-Magic" },
  { 13, "0D-BigMagic" },
  { 14, "0E-Chicken" },
  { 15, "0F-GreenSoldier" },
  { 16, "10-AliveRock" },
  { 17, "11-BlueSoldier" },
  { 18, "12-GroundBomb" },
  { 19, "13-Rupee-G" },
  { 20, "14-Fairy" },
  { 21, "15-Heart" },
  { 22, "16-Raven" },
  { 128, "80-Hole" },
  { 130, "82-Warp" },
  { 132, "84-Staircase" },
  { 134, "86-Bombable" },
  { 136, "88-Switch" },
};
const int kSecretEntriesCount = 28;

int FindSecretIndex(const char *name) {
  for (int i = 0; i < kSecretEntriesCount; i++) {
    if (strcmp(kSecretEntries[i].name, name) == 0) {
      return kSecretEntries[i].index;
    }
  }
  return -1;
}

