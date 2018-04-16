# test message
clear && echo "Testing gameserver! "
echo "     - checking server-client connection"
echo "     - checking inventory"
echo "     - checking waiting mode"
echo "     - checking closing room"
echo "     - checking chat continue after player is gone"

sleep 2;

# server
(xterm -e timeout --signal=SIGINT 110s ../gameserver "-p" "2" "-q" "10" "-i" "game_inventory.txt") &

# sleep 2 seconds
sleep 2;

# check players' connection
(xterm -e timeout --signal=SIGINT 50s ../player "-n" "Bobby"   "-i" "player1_inventory.txt" "$(hostname)") &
(xterm -e timeout --signal=SIGINT 50s ../player "-n" "Lola"  "-i" "player2_inventory.txt" "$(hostname)") &

# sleep 5 seconds
sleep 5;

# continue testing
(xterm -e timeout --signal=SIGINT 50s ../player "-n" "Sensitive"  "-i" "player3_inventory.txt" "$(hostname)") &
# check invalid data input
(xterm -e timeout --signal=SIGINT 20s ../player "-n" "Error"   "-i" "wrong_inventory.txt" "$(hostname)") &

# sleep 5 seconds
sleep 10;

# show chat continues after one player of room quits and waiting message
(xterm -e timeout --signal=SIGINT 50s ../player "-n" "President"  "-i" "player4_inventory.txt" "$(hostname)") &

# check waiting message
(xterm -e timeout --signal=SIGINT 30s ../player "-n" "Silvester"  "-i" "player5_inventory.txt" "$(hostname)") &

# wait for test 1 to end
sleep 55;