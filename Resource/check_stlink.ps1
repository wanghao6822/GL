Write-Host "=== ST-Link / STM32 USB Devices ==="
Get-PnpDevice -Class USB -PresentOnly | Where-Object { $_.FriendlyName -match 'STM|ST.Link|STLink|Debug' } | Format-Table FriendlyName, Status, InstanceId -AutoSize

Write-Host "`n=== All USB Serial / Debug Devices ==="
Get-PnpDevice -PresentOnly | Where-Object { $_.Class -eq 'Ports' -or $_.FriendlyName -match 'ST-Link|STLink|STM32|Virtual COM|USB Serial' } | Format-List FriendlyName, Status, InstanceId

Write-Host "`n=== Check for Zadig / WinUSB driver ==="
Get-WmiObject Win32_PnPSignedDriver | Where-Object { $_.DeviceName -match 'STM|ST.Link|STLink' } | Format-Table DeviceName, DriverVersion, DriverProviderName -AutoSize
