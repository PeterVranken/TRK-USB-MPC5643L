$usage = `
'usage: .\setEnv.ps1
  Set environment variables appropriately to run Eclipse and makefile based builds from PowerShell'

function endWithError { write-host $usage; exit; }

# Support help
#if($args[0] -match '^$|^(-h|/h|/\?|--help)$') {endWithError}

# Limit the allowed number of parameters.
#if($args[0] -match '.') {endWithError}

# The path to the installation of the Windows port of the unix shell tools. Note, this
# environment may already be persistently set by the installation procedure.
if ("$env:UNIX_TOOLS_HOME" -eq "")
{
    # TODO Adjust the path to the root folder of the installation of the UNIX tools (e.g. cygwin) and remove this comment and the next two lines
    write-host ("setEnv.ps1: You need to configure this script prior to first use: Specify location of UNIX tools for Windows")
    Write-Host "Press any key to continue ..."; $null = $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

    $env:UNIX_TOOLS_HOME = "C:\ProgramFiles\Git\usr"
    write-host ('Environment variable UNIX_TOOLS_HOME is set to ' + $env:UNIX_TOOLS_HOME)
}
else
{
    write-host ('Environment variable UNIX_TOOLS_HOME is already persistently set to ' `
                + $env:UNIX_TOOLS_HOME)
}

# The path to the GCC C cross compiler installation. Note, this environment may
# already be persistently set by the installation procedure.
if ("$env:GCC_POWERPC_HOME" -eq "")
{
    # TODO Adjust the path to the root folder of the GNU compiler and remove this comment and the next two lines
    write-host ("setEnv.ps1: You need to configure this script prior to first use: Specify location of GCC for PowerPC")
    Write-Host "Press any key to continue ..."; $null = $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

    $env:GCC_POWERPC_HOME = "C:\ProgramFiles\powerpc-eabivle-4.9.4"
    write-host ('Environment variable GCC_POWERPC_HOME is set to ' + $env:GCC_POWERPC_HOME)
}
else
{
    write-host ('Environment variable GCC_POWERPC_HOME is already persistently set to ' `
                + $env:GCC_POWERPC_HOME)
}


# Create a convenience shortcut to the right instance of the make processor.
set-alias -Name "make" `
          -Value ($env:GCC_POWERPC_HOME+"\bin\make.exe") `
          -Description "Run the GNU make processor, which is appropriate to build the software for the TRK-USB-MPC5643L board"

# Prepare the Windows search path for the run of the compilation tools.
$env:PATH = $env:GCC_POWERPC_HOME + "\bin" `
            + ";" + $env:UNIX_TOOLS_HOME + "\bin" `
            + ";" + $env:PATH
