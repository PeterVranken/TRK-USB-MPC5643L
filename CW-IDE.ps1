$usage = `
'usage: .\CW-IDE.ps1
  Open Code Warrior Eclipse IDE for build and debugging of project'

function endWithError { write-host $usage; exit; }

# Support help
#if($args[0] -match '^$|^(-h|/h|/\?|--help)$') {endWithError}

# Limit the allowed number of parameters.
#if($args[0] -match '.') {endWithError}

# Prepare required environment variables and system search path.
.\setEnv.ps1

# The Eclipse based Code Warrior IDE is started with a local, project owned workspace.
# TODO Specify the path to the Code Warrior IDE (i.e. the Eclipse executable) and remove this comment and the next two lines
write-host ("CW-IDE.ps1: You need to configure this script prior to first use: Specify location of Code Warrior IDE")
Write-Host "Press any key to continue ..."; $null = $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

C:\ProgramFiles\NXP\CW-MCU-v10.7\eclipse\cwide.exe -data .\workspaceCW