# Windows settings for local inference

Every Windows setting we changed on our test machines, why, and how to undo it. Our machines do nothing but this
work, so we can turn off things a normal PC keeps. Change only what fits your machine.

Each row says **Measured** when we have an A/B on our own hardware, or **Reason** when the change removes a known load
but we have no separate number for it. Run the commands in an **elevated PowerShell 7** window. Every service,
task, policy and power-setting name below is standard Windows: `Get-Service`, `Get-ScheduledTask` and `powercfg /q`
show each one on your own machine.

**After every Windows update, check these again.** An update can turn some of them back on.

## Biggest effect first

| Setting | Our value | Why | Evidence | Undo |
|---|---|---|---|---|
| Power plan | **Balanced** | On an APU the CPU and the integrated GPU share one power budget. "Performance" plans give it to the CPU and starve the GPU. | **Measured** (Radeon 780M, GPU decode): Balanced 29.12 t/s, Ultimate 28.90, High Performance 28.39; a "max performance" setup (minimum CPU state 100% plus the AMD power slider at max) **11.07 t/s** | `powercfg /setactive SCHEME_BALANCED` is the setting; to undo, pick your old plan |
| Hypervisor (Hyper-V, Virtual Machine Platform, Windows Hypervisor Platform) | **Off** | With a hypervisor running, every memory access the GPU makes pays an extra translation cost on the shared memory. | Reason | `bcdedit /set hypervisorlaunchtype auto`, re-add the Windows features, reboot |
| Virtualization-based security / memory integrity, Credential Guard | **Off** | Same shared-memory cost as the hypervisor (it needs one). | Reason (same cost as the hypervisor row) | Windows Security > Device security > Core isolation, turn it back on, reboot |
| Hardware-accelerated GPU scheduling (HAGS) | **On** | Every number we publish was measured with it on. | Reason (our baseline) | Settings > System > Display > Graphics > Default graphics settings |
| Microsoft Defender exclusions on the model, build and run folders | **Added** | Defender scans a file when a program opens it. A 20-100 GB model file then stalls the load for seconds. | Reason. A public study measured 18.9 s -> 8.1 s cold start on a 76 GB model by taking the scan out of the load path ([ga5in.com](https://ga5in.com/blog/2026-09-18-windows-devdrive-llm-refs/)) | `Remove-MpPreference -ExclusionPath '<folder>'` |
| NVMe deep sleep (APST) on mains power | **Off** | An idle NVMe drive drops into deep power states after 0.2 s and needs 10-45 ms to wake. Bursty reads (model loads, streamed experts) pay that each time. | **Measured** (KingSpec XG7000): bursty 1 MB reads 3.0 ms -> about 1.0 ms average latency | see the [NVMe guide](windows-native-nvme/README.md#also-turn-off-nvme-deep-sleep-desktops-on-mains-power) |
| Native NVMe driver (`nvmedisk.sys`) | **On**, all drives | Skips the SCSI translation layer. | **Measured**, see the [NVMe guide](windows-native-nvme/README.md) | in the guide |
| PCIe link power saving (ASPM) on mains power | **Off** | A sleeping PCIe link adds wake-up latency to scattered reads. | Reason | `powercfg /setacvalueindex SCHEME_CURRENT SUB_PCIEXPRESS ASPM 2; powercfg /setactive SCHEME_CURRENT` |
| Turn off hard disk after (mains power) | **Never** (0) | Drives never spin down or power off between requests. | Reason | `powercfg /setacvalueindex SCHEME_CURRENT SUB_DISK 6738e2c4-e8a5-4a42-b16a-e040e769756e 60; powercfg /setactive SCHEME_CURRENT` |

**Defender on a shared or company machine:** do not add folder exclusions there. Put the models on a **ReFS Dev Drive
on its own partition** instead: Defender then scans in the background without blocking the load, and protection stays
on. Do not use a Dev Drive inside a VHDX file for models: the study above measured -56% sequential read speed for it.

## Background load we turned off (frees RAM and CPU, keeps timing steady)

| Setting | Our value | Why | Evidence | Undo |
|---|---|---|---|---|
| Windows Search indexer (`WSearch`) | **Disabled** | It indexes new files in the background, and model and build folders hold many large new files. | Reason | `Set-Service WSearch -StartupType Automatic; Start-Service WSearch` |
| SysMain (Superfetch) | **Disabled** | It pre-loads files into free RAM. On an APU that RAM is also the GPU's memory pool. | Reason | `Set-Service SysMain -StartupType Automatic; Start-Service SysMain` |
| Windows on-device AI (Recall, Click to Do, AI data analysis, Copilot runtime, Settings agent) | **Off by policy** | Its host process (`WorkloadsSessionHost.exe`) holds RAM in the background, and on an APU that RAM is also the GPU's memory pool. | Reason; `Get-Process WorkloadsSessionHost` shows its memory on your machine | delete the values below or set them back |
| OneDrive (app autostart + its 8 scheduled tasks) | **Off** | Background sync. | Reason | start OneDrive once and tick "Start OneDrive when I sign in" |
| Fast Startup and hibernation | **Off** | Driver and setting changes then always apply at the next boot. | Reason | `powercfg /hibernate on` |
| AMD Software autostart (`StartCN`, `StartDVR` tasks) | **Disabled** | The desktop app and its recorder are not needed to run models; the driver keeps working. | Reason | `Enable-ScheduledTask -TaskName StartCN`, same for `StartDVR` |
| Background maintenance tasks | **Disabled** | Each can wake up during a measurement. | Reason | `Enable-ScheduledTask -TaskPath '<path>' -TaskName '<name>'` |

The background tasks we disabled: `\Microsoft\Windows\Feedback\Siuf\DmClient`, `...\DmClientOnScenarioDownload`,
`\Microsoft\Windows\Flighting\OneSettings\RefreshCache`, `\Microsoft\Windows\Windows Error Reporting\QueueReporting`,
the five `\Microsoft\Office\...` update and maintenance tasks, `\Adobe Acrobat Update Task`, and the SoftLanding
creative-management task. Only disable what is installed on your machine.

### The Windows AI policy values

```powershell
$k = 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsAI'
New-Item $k -Force | Out-Null
Set-ItemProperty $k AllowRecallEnablement 0 -Type DWord
Set-ItemProperty $k DisableClickToDo 1 -Type DWord
Set-ItemProperty $k DisableAIDataAnalysis 1 -Type DWord
Set-ItemProperty $k AllowCopilotRuntime 0 -Type DWord
Set-ItemProperty $k DisableSettingsAgent 1 -Type DWord
Get-Process WorkloadsSessionHost -ErrorAction SilentlyContinue | Stop-Process -Force
```

Undo: `Remove-Item 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsAI'` (or set each value back), then sign out and in.

### Turn off the services and the AMD autostart

```powershell
foreach ($s in 'WSearch','SysMain') { Stop-Service $s -Force; Set-Service $s -StartupType Disabled }
powercfg /hibernate off
Disable-ScheduledTask -TaskName StartCN; Disable-ScheduledTask -TaskName StartDVR
```

## Check after every update

```powershell
Get-Service WSearch, SysMain | Format-Table Name, StartType, Status
Get-ItemProperty 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsAI'
Get-Process WorkloadsSessionHost -ErrorAction SilentlyContinue
powercfg /getactivescheme
```
