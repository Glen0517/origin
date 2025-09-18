Set objShell = CreateObject("WScript.Shell")
Dim strDesktopPath
strDesktopPath = objShell.SpecialFolders("Desktop")

' 创建应用程序快捷方式
Set objShortcut = objShell.CreateShortcut(strDesktopPath & "\基金应用.lnk")
objShortcut.TargetPath = "d:\new_trea_project\start_fund_app.bat"
objShortcut.WorkingDirectory = "d:\new_trea_project"
objShortcut.WindowStyle = 1
objShortcut.IconLocation = "E:\python\python.exe"
objShortcut.Description = "基金应用 - 启动本地服务"
objShortcut.Save

WScript.Echo "快捷方式已创建到桌面！"