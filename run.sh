#!/bin/bash 
echo "check........"
if [ ! -e  /usr/local/share/bochs/keymaps/x11-pc-us.map ];then
    echo "/usr/local/share/bochs/keymaps/x11-pc-us.map does not exist..."
    exit 1
else
    file /usr/local/share/bochs/keymaps/x11-pc-us.map
fi

if [ ! -e  /usr/local/share/bochs/BIOS-bochs-latest ];then
    echo " /usr/local/share/bochs/BIOS-bochs-latest does not exist..."
    exit 1
else 
    file /usr/local/share/bochs/BIOS-bochs-latest
fi

if [ ! -e  /usr/share/vgabios/vgabios.bin ];then
    echo "/usr/share/vgabios/vgabios.bin does not exist..."
    exit 1
else
    file /usr/share/vgabios/vgabios.bin
fi

if [ -e hd80M.img ];then
    echo -e  "n\np\n1\n\n+4M\nn\ne\n2\n\n\nn\n\n+5M\nn\n\n+6M\nn\n\n+7M\nn\n\n+8M\nn\n\n+9M\nn\n\n\nw\n" | fdisk hd80M.img &> /dev/null
else
    echo "no hd80M.img!"
    exit 1
fi

echo "check over ...."
sleep 1
echo "run........"
bochs -f bochsrc.disk