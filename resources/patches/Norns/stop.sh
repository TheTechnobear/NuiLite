#/bin/bash
echo stopping norns
sudo systemctl stop norns-matron.service
sudo systemctl stop norns-sclang.service
sudo systemctl stop norns-crone.service
sudo systemctl stop norns-jack.service

