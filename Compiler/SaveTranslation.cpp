extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <SlyvQCol.hpp>
#include <JCR6_Write.hpp>

#include "Translate.hpp"


using namespace Slyvina::Units;
using namespace Slyvina::JCR6;

namespace Scyndi {

    static bool err{ false };

    std::shared_ptr<std::vector<char>> OutBuf{ nullptr };

    static int LuaPaniek(lua_State* L) {
        QCol->Error("Compiling Lua translation failed");
        //std::string Trace{};
        //Error("Lua Error!");
        QCol->Doing("Lua#", lua_gettop(L));
        for (int i = 1; i <= lua_gettop(L); i++) {
            QCol->Magenta(TrSPrintF( "Arg #%03d\t", i));
            switch (lua_type(L, i)) {
            case LUA_TSTRING:
                QCol->Doing("String", "\"" + std::string(luaL_checkstring(L, i)) + "\"");
                //Trace += luaL_checkstring(L, i); Trace += "\n";
                break;
            case LUA_TNUMBER:
                QCol->Doing("Number ", std::to_string(luaL_checknumber(L, i)));
            case LUA_TFUNCTION:
                QCol->Doing("Function","<>");
            default:
                QCol->Doing("Unknown: " , lua_type(L, i));
                break;
            }
            ///cout << "\n";
        }
        //Error("", false, true);
        //exit(11);
        err = true;
        return 0;
    }

    static int DumpLua(lua_State* L, const void* p, size_t sz, void* ud) {
       
        auto pcp{ (char*)p };
        for (size_t i = 0; i < sz; ++i) OutBuf->push_back(pcp[i]);
        return 0;
    }

	bool SaveTranslation(Translation Trans, JT_Create Out,std::string Storage) {
        auto L{ luaL_newstate() };
        auto source{ Trans->LuaSource };
        QCol->Doing("Compiling", "Lua translation");
        err = false;
        luaL_openlibs(L);
        lua_atpanic(L, LuaPaniek);
        luaL_loadstring(L, source.c_str());
        //lua_call(L, 0, 0);
        if (!err) {
            OutBuf = std::make_shared < std::vector < char > >();
            lua_dump(L, DumpLua, NULL, 0);
        }
        if (Out) {
            QCol->Doing("Writing", "Bytecode");
            if (OutBuf->size() > 2048)
                Out->AddChars(*OutBuf, "ByteCode.lbc", Storage);
            else
                Out->AddChars(*OutBuf, "ByteCode.lbc");
            QCol->Doing("Writing", "Translation");
            if (source.size() > 2048)
                Out->AddString(source, "Translation.lua", Storage);
            else
                Out->AddString(source, "Translation.lua");
            QCol->Doing("Writing", "Configuration");
            auto UPD{ Trans->Data->UnParse() };
            if (UPD.size() > 2048)
                Out->AddString(UPD, "Configuration.ini", Storage);
            else
                Out->AddString(UPD, "Configuration.ini");

        } else {
            //cout << "Writing:   " << dest << endl;
            //auto BT{ WriteFile(dest) };
            //for (auto ch : OutBuf) BT->Write(ch);
            //BT->Close();
        }
        lua_close(L);
        return !err;
	}

}