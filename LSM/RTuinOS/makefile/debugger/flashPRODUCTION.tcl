# Flash script for CodeWarrior IDE.
#   The idea has been adopted from
# https://mcuoneclipse.com/2012/08/02/standalone-flash-programmer/, downloaded on Nov 4,
# 2017. The actual set of commands has been taken by Copy & Paste from the IDE's console
# window after running the built-in flash tool.
#   In the CodeWarrior IDE open the debugger shell to run this script. Click menu
# Window/Show View/Debugger Shell to dos so. In the debugger shell window, type:
#
#   cd ../LSM/RTuinOS/makefile/debugger (depending on where you are)
#   pwd
#   dir
#   set APP tc01
#   source flashPRODUCTION.tcl
#
# Note, tc01 is an example only. It designates the name of the sample application, which is
# compiled with RTuinOS. (RTuinOS is a library only but not a flashable, self-contained
# application.)
#
# Note, this script requires that the working directory is the directory where this script
# is located. Use the TCL commands pwd and dir in the debugger shell to find out.

if {$APP != ""} {
    puts "RTuinOS is flashed with application $APP"

    # In any case, disconnect an existing debug connection.
    fl::disconnect

    # Set launch configuration.
    fl::target -lc "RTuinOS $APP (PRODUCTION)"

    # Set the target RAM buffer for downloading image data.
    fl::target -b 0x40000000 0x20000

    # Switch off verify and logging.
    fl::target -v off -l off

    # Select flash device, organization and memory range.
    cmdwin::fl::device -d "MPC5643L_BOOKE" -o "1Mx32x1" -a 0x0 0xfffff

    # Unprotect the device. This is essential the very first time with a new device, but
    # later only if we protect it again after flashing. This script doesn't protect again,
    # so you may consider commenting out this line after first flashing in order to speed
    # up the procedure.
    cmdwin::fl::protect all off

    # Specify target file, auto detect format, range settings on followed by the flash
    # range, offset settings off.
    cmdwin::fl::image -f ..\\..\\bin\\ppc\\$APP\\PRODUCTION\\TRK-USB-MPC5643L-RTuinOS-$APP.elf -t "Auto Detect" -re on -r 0x0 0xfffff -oe off

    # Now erase the flash...
    cmdwin::fl::erase image

    # ... followed by writing the application to flash.
    cmdwin::fl::write

    # Reset of device to start the flashed application is unfortunately not supported for
    # the MPC5643L.
    #mc::reset

    # Disconnect connection.
    fl::disconnect
    
    # We print the name of the RTuinOS application once again; it happens so easy that $APP
    # is not set and the status line at the beginning has scrolled away.
    puts "RTuinOS has been flashed with application $APP in PRODUCTION compilation"
    
} else {
    puts "Error: Please set TCL variable tc prior to running this script."
    puts "tc should contain the name of an RTuinOS application, which has been compiled"
    puts "with RTuinOS. RTuinOS on its own is not a self-contained, flashable application"
}
