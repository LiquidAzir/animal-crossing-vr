# Manual Quest setup and save backups

The Windows installer is the simplest route. These commands are for users with **ADB and Python 3** already installed, including macOS/Linux. `device_data.py` is included in the Quest zip.

Enable Developer Mode and authorize USB debugging first. Run `adb devices`, replace `YOUR_QUEST_SERIAL` below, and close Animal Crossing on the headset. Use your own **USA Rev 0 / GAFE01_00** ISO, GCM, or CISO.

```sh
adb -s YOUR_QUEST_SERIAL install -r AnimalCrossing-Quest.apk
python3 device_data.py --adb /path/to/adb --serial YOUR_QUEST_SERIAL --rom /path/to/AnimalCrossing.iso
```

Then open **Animal Crossing** from Unknown Sources. When using the separate APK download, substitute its downloaded filename in the install command. The helper creates private app directories under the correct owner and verifies the transfer; do not copy ROMs into Android/data with a file manager.

## Back up or copy a save (optional)

Close the game first. Export to a **new directory** each time:

```sh
python3 device_data.py --adb /path/to/adb --serial YOUR_QUEST_SERIAL --export-save /path/to/new-quest-backup
```

To bring a copy of a PC `.gci` save into an empty Quest save slot:

```sh
python3 device_data.py --adb /path/to/adb --serial YOUR_QUEST_SERIAL --save /path/to/DobutsunomoriP_MURA.gci
```

The helper refuses to overwrite different existing files. Keep PC and Quest saves separate; they do not sync. Do not import PC settings into Quest. Updating with the same signed APK preserves app data; uninstalling or clearing storage deletes it.
