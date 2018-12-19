# Flash script for CodeWarrior IDE.
#   The idea has been adopted from
# https://mcuoneclipse.com/2012/08/02/standalone-flash-programmer/, downloaded on Nov 4,
# 2017. The actual set of commands has been taken by Copy & Paste from the IDE's console
# window after running the built-in flash tool.
#   In the CodeWarrior IDE open the debugger shell to run this script. Click menu
# Window/Show View/Debugger Shell to do so. In the debugger shell window, type:
#
#   cd ../LSM/kernelBuilder/makefile/debugger (depending on where you are)
#   pwd
#   dir
#   source flashAlternatingContextsPRODUCTION.tcl
#
#   According to
# https://mcuoneclipse.com/2015/08/31/programming-kinetis-with-codewarrior-from-the-dos-shell/,
# downloaded Nov 4, 2017, the script can be run from the command line to enable automation
# of flashing (e.g. by makefile rule). This requires a "quitIDE" as last TCL command in the
# script and the command line would resemble:
#
#   cwide.exe -data "c:\tmp\wsp_StandaloneFlsh" -vmargsplus -Dcw.script=(...)\TRK-USB-MPC5643L\LSM\kernelBuilder\makefile\debugger\flashAlternatingContextsPRODUCTION.tcl
#
# However, we couldn't make this work.
#   More related links are:
# https://mcuoneclipse.com/2012/04/12/scripting-the-debugger-shell-getting-started/ 
# https://mcuoneclipse.com/2012/07/29/debugger-shell-test-automation/ 
# https://mcuoneclipse.com/2013/10/26/eclipse-command-line-code-generation-with-processor-expert/ 
# https://mcuoneclipse.com/2012/08/03/codewarrior-flash-programming-from-a-dos-shell/
# https://mcuoneclipse.com/2012/08/02/standalone-flash-programmer/ 
# https://mcuoneclipse.com/2015/08/31/programming-kinetis-with-codewarrior-from-the-dos-shell/ 

# Note, this script requires that the working directory is the directory where this script
# is located. Use the TCL commands pwd and dir in the debugger shell to find out.

# In any case, disconnect an existing debug connection.
fl::disconnect
 
# Set launch configuration.
fl::target -lc "kernelBuilder (alternatingContexts, Book E, PRODUCTION)"
 
# Set the target RAM buffer for downloading image data.
fl::target -b 0x40000000 0x20000
 
# Switch off verify and logging.
fl::target -v off -l off
 
# Select flash device, organization and memory range.
cmdwin::fl::device -d "MPC5643L_BOOKE" -o "1Mx32x1" -a 0x0 0xfffff
 
# Unprotect the device. This is essential the very first time with a new device, but later
# only if we protect it again after flashing. This script doesn't protect again, so you may
# consider commenting out this line after first flashing in order to speed up the procedure.
cmdwin::fl::protect all off

# Specify target file, auto detect format, range settings on followed by the flash range,
# offset settings off.
cmdwin::fl::image -f ..\\..\\bin\\ppc-BookE\\alternatingContexts\\PRODUCTION\\TRK-USB-MPC5643L-kernelBuilder.elf -t "Auto Detect" -re on -r 0x0 0xfffff -oe off
 
# Now erase the flash...
cmdwin::fl::erase image
 
# ... followed by writing the application to flash.
cmdwin::fl::write
 
# Reset of device to start the flashed application is unfortunately not supported for the
# MPC5643L. 
#mc::reset

# Disconnect connection.
fl::disconnect

# Close the CodeWarrior IDE application. (Uncomment this command if you are going to run
# the IDE as command line tool for flashing: IDE should execute script and terminate.)
#quitIDE
