# Flash script for CodeWarrior IDE.
#   The idea has been adopted from
# https://mcuoneclipse.com/2012/08/02/standalone-flash-programmer/, downloaded on Nov 4,
# 2017. The actual set of commands has been taken by Copy & Paste from the IDE's console
# window after running the built-in flash tool.
#   In the CodeWarrior IDE open the debugger shell to run this script. Click menu
# Window/Show View/Debugger Shell to do so. In the debugger shell window, type:
#
#   cd ../LSM/safe-RTOS-VLE/makefile/debugger (depending on where you are)
#   pwd
#   dir
#   set APP default
#   source flashDEBUG.tcl
#
#   Note, "default" is an example only. It designates the sample application, which is
# compiled with safe-RTOS-VLE. (safe-RTOS-VLE is a library only but not a flashable,
# self-contained application.)
#   Note, this script requires that the working directory is the directory where this
# script is located. Use the TCL commands pwd and dir in the debugger shell to find out.

if {[info exists APP]  &&  $APP!=""} {
    puts "safe-RTOS-VLE application $APP is being flashed"

    # In any case, disconnect an existing debug connection.
    fl::disconnect

    # Set launch configuration.
    fl::target -lc "safe-RTOS-VLE ($APP, DEBUG)"

    # Set the target RAM buffer for downloading image data.
    fl::target -b 0x40000000 0x20000

    # Switch off verify and logging.
    fl::target -v off -l off

    # Select flash device, organization and memory range.
    cmdwin::fl::device -d "MPC5643L_VLE" -o "1Mx32x1" -a 0x0 0xfffff

    # Unprotect the device. This is essential the very first time with a new device, but
    # later only if we protect it again after flashing. This script doesn't protect again,
    # so you may consider commenting out this line after first flashing in order to speed
    # up the procedure.
    cmdwin::fl::protect all off

    # Specify target file, auto detect format, range settings on followed by the flash
    # range, offset settings off.
    cmdwin::fl::image -f ..\\..\\bin\\ppc\\$APP\\DEBUG\\TRK-USB-MPC5643L-safe-RTOS-VLE.elf -t "Auto Detect" -re on -r 0x0 0xfffff -oe off

    # Now erase the flash...
    cmdwin::fl::erase image

    # ... followed by writing the application to flash.
    cmdwin::fl::write

    # Protect the flash contents of the device again. Uncomment it if suitable. (During
    # development it is not essential to have the protection.)
    cmdwin::fl::protect all on
    
    # Reset of device to start the flashed application is unfortunately not supported for
    # the MPC5643L. 
    #mc::reset

    # Disconnect connection.
    fl::disconnect

    # We print the name of the RTOS application once again; it happens so easy
    # that $APP is not set and the status line at the beginning has scrolled away.
    puts "safe-RTOS-VLE application $APP has been flashed in DEBUG compilation"
    
} else {
    puts "Error: Please set TCL variable APP prior to running this script."
    puts "  APP should contain the name of an application, which has been compiled"
    puts "with safe-RTOS-VLE. safe-RTOS-VLE on its own is not a self-contained,"
    puts "flashable application."
    puts "  See the source code of this script for an example."
}
