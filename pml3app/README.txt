Instructions for Building and Transferring to Development Feig Reader

System Setup

- Make sure the development system is Ubuntu 14 and pc setup as instructed by Feid SDK manual
- Connect the Fieg Dongle to the development machine
- Make sure to have VSCode installed to open and edit easily or any notepad
- Install shpass which allows to copy to the board without asking for password every time
	- using sudo apt-get install sshpass
	- or Follow https://www.tecmint.com/sshpass-non-interactive-ssh-login-shell-script-ssh-password/


Build and Deploy

- Extract the source code in a folder
- Folder can be opened using Visual Studio for Code
- Open the Makefile
- Edit the following as required
	DEVICE = 192.168.245.210
	SLOTAPP = app0
	KEYID = 00A0
	PIN = 648219
- Run the make command
- This will build the project, generate l3app and then copy all the required files to the board IP
- SSH to the board
- Give Execute Permission : chmod +x to l3app and l3start.sh
- App can be run in the boarding using ./l3start.sh
