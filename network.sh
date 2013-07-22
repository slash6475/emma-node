RADVD_FILE="/etc/radvd.conf"
RADVD_ETH="usb0"
PREFIX="BBBB"


echo "
interface $RADVD_ETH
{
   AdvSendAdvert on;
   AdvLinkMTU 1280;
   AdvCurHopLimit 128;
   AdvReachableTime 360000;
   MinRtrAdvInterval 100;
   MaxRtrAdvInterval 150;
   AdvDefaultLifetime 200;
   prefix $PREFIX::/64
   {
       AdvOnLink on;
       AdvAutonomous on;
       AdvPreferredLifetime 4294967295;
       AdvValidLifetime 4294967295;
   };
};" > $RADVD_FILE

echo "***** lowPAN starting *****"
ifconfig $RADVD_ETH up

echo "Configure the IP addresses on usb0"
ip -6 address add fe80::0012:13ff:fe14:1516/64 scope link dev $RADVD_ETH
ip -6 address add $PREFIX::1/64 dev $RADVD_ETH

echo "Activating of IPV6 forwarding"
echo 1 > /proc/sys/net/ipv6/conf/all/forwarding

echo "Radvd starting"
pkill radvd
radvd start

