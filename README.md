# Simple Wake On LAN
 This project is based on https://github.com/kabcode/WakeOnLAN repository, but modified to work much better on modern networks. This project will compile using the MinGW GCC compiler, and is also configured to broadcast the magic packet on all interface broadcast addresses. This is because modern OSes seem to use local loopback as the primary special broadcast (255.255.255.255) causing the magic packet not to be heard by the entire network.
 
 
 # Build
 
 Make sure you have properly configured your environment PATH variable for your MinGW GCC installation.
 
 Run the *build.bat* script to build all files and generate the *wol.exe* application.
