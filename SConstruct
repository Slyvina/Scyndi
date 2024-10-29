import platform
from datetime import date
from os import system
from os import environ

Today = date.today().strftime("%b %d %y")

print("\x1b[32mScyndi Compiler\x1b[33m\tBuilder script\x1b[0m\n(c) Jeroen P. Broks\n")
def Doing(a,b): print("\x1b[93m%s: \x1b[96m%s\x1b[0m"%(a,b))
Doing("Platform",platform.system())
print("\n\n")

# Core Dump
Doing("Creating","Core Dump")
if platform.system()=="Windows":
    system("pwsh ScyndiCoreDump.ps1")
else:   
    # Strictly speaking a PowerShell script, but the code of this particular script is identical in both PowerShell as SH so why bother?
    system("sh ScyndiCoreDump.ps1") 

# Base Functions
Files = []
Libs = []

def Add(cat,files):
    for f in files: 
        Doing(cat,f)
        Files.append(f)

def AddLib(cat,files):
    for f in files: 
        Doing("Lib "+cat,f)
        Libs.append(f)

Add("   JCR6",[
    "../../Libs/JCR6/Source/JCR6_Core.cpp",
    "../../Libs/JCR6/Source/JCR6_RealDir.cpp",
    "../../Libs/JCR6/Source/JCR6_Write.cpp",
    "../../Libs/JCR6/Source/JCR6_zlib.cpp"])
if platform.system()!="Windows":
    Add("   zlib",Glob("../../Libs/JCR6/3rdParty/zlib/src/*.c"))
LuaFiles = [    
    "../../Libs/Lunatic/Lua/Raw/src/lapi.c",
    "../../Libs/Lunatic/Lua/Raw/src/lauxlib.c",
    "../../Libs/Lunatic/Lua/Raw/src/lbaselib.c",
    "../../Libs/Lunatic/Lua/Raw/src/lcode.c",
    "../../Libs/Lunatic/Lua/Raw/src/lcorolib.c",
    "../../Libs/Lunatic/Lua/Raw/src/lctype.c",
    "../../Libs/Lunatic/Lua/Raw/src/ldblib.c",
    "../../Libs/Lunatic/Lua/Raw/src/ldebug.c",
    "../../Libs/Lunatic/Lua/Raw/src/ldo.c",
    "../../Libs/Lunatic/Lua/Raw/src/ldump.c",
    "../../Libs/Lunatic/Lua/Raw/src/lfunc.c",
    "../../Libs/Lunatic/Lua/Raw/src/lgc.c",
    "../../Libs/Lunatic/Lua/Raw/src/linit.c",
    "../../Libs/Lunatic/Lua/Raw/src/liolib.c",
    "../../Libs/Lunatic/Lua/Raw/src/llex.c",
    "../../Libs/Lunatic/Lua/Raw/src/lmathlib.c",
    "../../Libs/Lunatic/Lua/Raw/src/lmem.c",
    "../../Libs/Lunatic/Lua/Raw/src/loadlib.c",
    "../../Libs/Lunatic/Lua/Raw/src/lobject.c",
    "../../Libs/Lunatic/Lua/Raw/src/lopcodes.c",
    "../../Libs/Lunatic/Lua/Raw/src/loslib.c",
    "../../Libs/Lunatic/Lua/Raw/src/lparser.c",
    "../../Libs/Lunatic/Lua/Raw/src/lstate.c",
    "../../Libs/Lunatic/Lua/Raw/src/lstring.c",
    "../../Libs/Lunatic/Lua/Raw/src/lstrlib.c",
    "../../Libs/Lunatic/Lua/Raw/src/ltable.c",
    "../../Libs/Lunatic/Lua/Raw/src/ltablib.c",
    "../../Libs/Lunatic/Lua/Raw/src/ltm.c",
    "../../Libs/Lunatic/Lua/Raw/src/lundump.c",
    "../../Libs/Lunatic/Lua/Raw/src/lutf8lib.c",
    "../../Libs/Lunatic/Lua/Raw/src/lvm.c",
    "../../Libs/Lunatic/Lua/Raw/src/lzio.c"]
Add("    Lua",LuaFiles)    
Add("  Units",[
    "../../Libs/Units/Source/SlyvArgParse.cpp",
    "../../Libs/Units/Source/SlyvAsk.cpp",
    "../../Libs/Units/Source/SlyvBank.cpp",
    "../../Libs/Units/Source/SlyvDir.cpp",
    "../../Libs/Units/Source/SlyvQCol.cpp",
    "../../Libs/Units/Source/SlyvSTOI.cpp",
    "../../Libs/Units/Source/SlyvStream.cpp",
    "../../Libs/Units/Source/SlyvString.cpp",
    "../../Libs/Units/Source/SlyvMD5.cpp",
    "../../Libs/Units/Source/SlyvTime.cpp",
    "../../Libs/Units/Source/SlyvVolumes.cpp"])    
Add(" Scyndi",[
    "Compiler/Config.cpp",
    "Compiler/KeyWords.cpp",
    "Compiler/SaveTranslation.cpp",
    "Compiler/ScyndiGlobals.cpp",
    "Compiler/ScyndiProject.cpp",
    "Compiler/Translate.cpp",
    "Scyndi.cpp"])
print("\x1b[95mAll source file accounted for!\x1b[0m")

IncludeDirs = ["../../Libs/JCR6/Headers","../../Libs/Lunatic","../../Libs/Lunatic/Lua/Raw/src","../../Libs/Units/Headers","../../Libs/JCR6/3rdParty/zlib/src/"]

QFiles = [
    "../../Libs/JCR6/Source/JCR6_Core.cpp",
    "../../Libs/JCR6/Source/JCR6_zlib.cpp",
    "../../Libs/Lunatic/Lunatic.cpp",
    "../../Libs/Units/Source/SlyvBank.cpp",
    "../../Libs/Units/Source/SlyvQCol.cpp",
    "../../Libs/Units/Source/SlyvSTOI.cpp",
    "../../Libs/Units/Source/SlyvStream.cpp",
    "../../Libs/Units/Source/SlyvString.cpp",
    "../../Libs/Units/Source/SlyvTime.cpp",
    "../../Libs/Units/Source/SlyvVolumes.cpp",
    "QuickScyndi/QuickScyndi.cpp"
] + LuaFiles + Glob("../../Libs/JCR6/3rdParty/zlib/src/*.c")


Program("Exe/%s/scyndi"%platform.system(),Files,CPPPATH=IncludeDirs)
Program("Exe/%s/quickscyndi"%platform.system(),QFiles,CPPPATH=IncludeDirs)