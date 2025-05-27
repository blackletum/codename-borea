rem Put a copy command in here to copy your client.dll into your mod directory
robocopy ".\projects\vs2019\Debug\hl_cdll" "C:\Program Files (x86)\Steam\steamapps\common\Half-Life\the_last_goodbye\cl_dlls" client.dll /njh /njs /ndl /nc /ns /np
robocopy ".\projects\vs2019\Debug\hl_cdll" "C:\Program Files (x86)\Steam\steamapps\common\Half-Life\the_last_goodbye\cl_dlls" client.pdb /njh /njs /ndl /nc /ns /np
