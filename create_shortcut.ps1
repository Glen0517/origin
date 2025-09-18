# 创建桌面快捷方式的PowerShell脚本
$WshShell = New-Object -ComObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut($WshShell.SpecialFolders.Item("Desktop") + "\基金应用.lnk")
$Shortcut.TargetPath = "d:\new_trea_project\start_fund_app.bat"
$Shortcut.WorkingDirectory = "d:\new_trea_project"
$Shortcut.IconLocation = "E:\python\python.exe"
$Shortcut.Description = "基金应用 - 启动本地服务"
$Shortcut.Save()

Write-Host "快捷方式已创建到桌面！"