#!/bin/bash
# Raspberry Pi Zero Low Power Setup Script

echo ">>> HDMI kapatılıyor..."
if ! grep -q "tvservice -o" /etc/rc.local; then
  sudo sed -i '/^exit 0/i /usr/bin/tvservice -o' /etc/rc.local
fi

echo ">>> LED'ler kapatılıyor..."
sudo sed -i '/dtparam=act_led_trigger/d' /boot/config.txt
sudo sed -i '/dtparam=act_led_activelow/d' /boot/config.txt
echo "dtparam=act_led_trigger=none" | sudo tee -a /boot/config.txt
echo "dtparam=act_led_activelow=on" | sudo tee -a /boot/config.txt

echo ">>> Bluetooth devre dışı bırakılıyor..."
if ! grep -q "disable-bt" /boot/config.txt; then
  echo "dtoverlay=disable-bt" | sudo tee -a /boot/config.txt
fi
sudo systemctl disable --now bluetooth.service hciuart.service

echo ">>> Wi-Fi güç tasarrufu aktif ediliyor..."
sudo mkdir -p /etc/NetworkManager/conf.d
echo -e "[connection]\nwifi.powersave = 3" | sudo tee /etc/NetworkManager/conf.d/wifi-powersave.conf

echo ">>> Gereksiz servisler devre dışı bırakılıyor..."
sudo systemctl disable --now \
  triggerhappy.service \
  cron.service \
  dphys-swapfile.service \
  keyboard-setup.service \
  rsyslog.service \
  cups.service \
  motd-news.service 2>/dev/null

echo ">>> Avahi (mDNS) tamamen kapatılıyor..."
sudo systemctl disable --now avahi-daemon.service avahi-daemon.socket

echo ">>> CPU güç yönetimi ayarlanıyor..."
sudo apt install -y cpufrequtils
echo 'GOVERNOR="powersave"' | sudo tee /etc/default/cpufrequtils
sudo cpufreq-set -g powersave
sudo cpufreq-set -d 300MHz -u 700MHz

echo ">>> Kernel modülleri kara listeye alınıyor..."
sudo bash -c 'cat > /etc/modprobe.d/blacklist.conf <<EOF
blacklist snd_bcm2835
blacklist bcm2835_v4l2
EOF'

echo ">>> Log boyutu sınırlandırılıyor..."
sudo sed -i '/SystemMaxUse/d' /etc/systemd/journald.conf
echo "SystemMaxUse=10M" | sudo tee -a /etc/systemd/journald.conf

echo ">>> Boot parametreleri düzenleniyor..."
if ! grep -q "quiet console=tty1" /boot/cmdline.txt; then
  sudo sed -i 's/$/ quiet console=tty1/' /boot/cmdline.txt
fi

echo ">>> Ayarlar tamamlandı. Sistem 3 saniye içinde yeniden başlatılacak..."
sleep 3
sudo reboot