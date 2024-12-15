set -e
# You need to set PROJECTPATH to the location containing the server directory.
PROJECTPATH=/home/minseok/2021_ite4065_2016025650/project4/mariadb

BASEPATH=$PROJECTPATH/server/inst
DATAPATH=$PROJECTPATH/data
MYCNFPATH=$PROJECTPATH/server/my.cnf
MY_USER=$USER

rm -rf $BASHPATH $DATAPATH

cmake \
	-DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$BASEPATH \
    -DMYSQL_DATADIR=$DATAPATH \
    -DMYSQL_UNIX_ADDR=$BASEPATH/mysql.sock \
    -DSYSCONFDIR=$BASEPATH/etc \
    -DMYSQL_USER=$MY_USER \
    -DDEFAULT_CHARSET=utf8 \
    -DDEFAULT_COLLATION=utf8_general_ci \
    -DWITH_EXTRA_CHARSETS=all \
    -DENABLED_LOCAL_INFILE=1 \
    -DWITH_SSL=system \
    -DWITH_ZLIB=system \

make -j2
make install

cd $BASEPATH
cp $MYCNFPATH ./

./scripts/mysql_install_db \
    --basedir=$BASEPATH \
    --datadir=$DATAPATH \
    --defaults-file=my.cnf \
    --skip-name-resolve \
    --user=$MY_USER \
    --verbose
