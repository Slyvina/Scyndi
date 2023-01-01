# Scyndi

![Scyndi Transparent background 3x zo groot](https://user-images.githubusercontent.com/11202073/209580803-e6ac9a72-4a2f-401f-baa3-08d9640342ff.png)


Scyndi is a kind of extension language to Lua.
The big difference with Neil is that Neil translates real-time, meaning all scripts are loaded as source, then translated to Lua and then compiled to Lua byte code. 
Scyndi will translate to Lua independently and depending on how you set up the compiler even call Lua to pre-compile the translated script into ready to go byte code. The idea is that this should shorten loading times in games significantly (as compiling scripts is the operation that appears to take most of the time during loading).

This is a WIP, so not much to see here YET!



In order not to break with traditons, below are a few examples of "Hello World"
~~~~
Script

Init
  Print("Hello World!")
End
~~~~

~~~
Script

Init
  Cout("Hello World!\n")
End
~~~

~~~
Script

Void HelloWorld()
  Print("Hello World!")
End

Init
  HelloWorld()
End
~~~
