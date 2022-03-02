# Genshin Impact FPS Unlocker
 - This tool helps you to unlock the 60 fps limit in the game
 - This is an external program that uses **WriteProcessMemory** to write the desired fps to the game
 - Handle protection bypass is already included
 - Does not require a driver for R/W access
 - Supports OS and CN version
 - Should work for future updates

 ## Compiling
 - Install x86_64-w64-mingw32-g++
 - Setup posix threads using `sudo update-alternatives --config x86_64-w64-mingw32-g++`
 - Run build.bat
 
 ## Usage
 - Make sure you have the [Visual C++ 2019 Redistributable (x64)](https://aka.ms/vs/16/release/vc_redist.x64.exe) installed
 - If it is your first time running, run unlocker as admin, then the unlocker will ask you to open the game. This only need to be done once, it's used for acquiring the game path. Then it'll be saved to a config file. After the config is made you can start the game via the unlocker in future sessions.
 - Place the compiled exe anywhere you want
 - Make sure your game is closed, the unlocker will automatically start the game for you
 - Run the exe as administrator, and leave the exe running
 >It requires adminstrator because the game needs to be started by the unlocker and the game requires such permission

 ## Notes
 - Tested on a new account for 6 months and no bans (AR55). Unable to guarantee it will be safe forever
 - Use it at your own risk
