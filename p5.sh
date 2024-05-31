setenv target 3_conn
rm -rf tmp

git clone https://github.com/James24029775/tmp.git
cd ./tmp/$target
make

echo "$target start"
./socks_server 11111
