# PowerShell script to create desktop shortcut
$WshShell = New-Object -ComObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut($WshShell.SpecialFolders.Item("Desktop") + "\FundApp.lnk")
$Shortcut.TargetPath = "d:\new_trea_project\start_fund_app.bat"
$Shortcut.WorkingDirectory = "d:\new_trea_project"
$Shortcut.IconLocation = "E:\python\python.exe"
$Shortcut.Description = "Fund Application - Start Local Server"
$Shortcut.Save()

Write-Host "Shortcut created on desktop!"