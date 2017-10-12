$usage = `
'usage: .\setEnv.ps1
  Set environment variables appropriately to run Eclipse and makefile based builds from PowerShell'

function endWithError { write-host $usage; exit; }

# Support help
#if($args[0] -match '^$|^(-h|/h|/\?|--help)$') {endWithError}

# Limit the allowed number of parameters.
#if($args[0] -match '.') {endWithError}

# The path to the GCC C cross compiler installation. Note, this environment may
# already be persistently set by the installation procedure.
if ("$env:PPC_GNU_BASEDIR" -eq "")
{
    # TODO Adjust the path to the root folder of the GNU compiler and remove this comment and the next two lines
    write-host ("setEnv.ps1: You need to configure this script prior to first use: Specify location of GCC for PowerPC")
    Write-Host "Press any key to continue ..."; $null = $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

    $env:PPC_GNU_BASEDIR = "C:\ProgramFiles\MinGW-powerpc-eabivle-4.9.4"
    write-host ('Environment variable PPC_GNU_BASEDIR is set to ' + $env:PPC_GNU_BASEDIR)
}
else
{
    write-host ('Environment variable PPC_GNU_BASEDIR is already persistently set to ' `
                + $env:PPC_GNU_BASEDIR)
}

# Create a convenience shortcut to the right instance of the make processor.
set-alias -Name "make" `
          -Value ($env:PPC_GNU_BASEDIR+"\bin\mingw32-make.exe") `
          -Description "Run the GNU make processor, which is appropriate to build the software for the TRK-USB-MPC%&$�L board"

# Prepare the Windows search path for the run of the HighTec tools.
$env:PATH = $env:PPC_GNU_BASEDIR + "\bin" `
            + ";" + $env:PATH