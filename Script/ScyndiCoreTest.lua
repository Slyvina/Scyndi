-- <License Block>
-- ***********************************************************
-- Script/ScyndiCoreTest.lua
-- This particular file has been released in the public domain
-- and is therefore free of any restriction. You are allowed
-- to credit me as the original author, but this is not
-- required.
-- This file was setup/modified in:
-- 2022
-- If the law of your country does not support the concept
-- of a product being released in the public domain, while
-- the original author is still alive, or if his death was
-- not longer than 70 years ago, you can deem this file
-- "(c) Jeroen Broks - licensed under the CC0 License",
-- with basically comes down to the same lack of
-- restriction the public domain offers. (YAY!)
-- ***********************************************************
-- Version 22.12.21
-- </License Block>
--[[

	This script only serves to test the core script
	(All Scyndi translated scripts need this script
	as a basis for their own opersations)

]]

require "ScyndiCore" 

Scyndi.Globals.Print("Hello World!")
Scyndi.Globals.Cout("Hallo Welt!",Scyndi.Globals.endl)
Scyndi.Globals.Cout("Salut ","monde!","\n")
Scyndi.Globals.PrintF("%s %s!\n","Hallo","Wereld")

a = { [0]="A", "B", "C", "D"}
for i,v in Scyndi.Globals.ipairs(a) do print(i,v) end
for i,v in Scyndi.Globals.pairs(a) do print(i,v) end