usage(){
	echo "Usage ./build.sh version_number"
	exit 0
}

#Add install files

sudo apt update
sudo apt install debmake devscripts debhelper dh-autoreconf automake libtool autoconf autoconf-archive -y


if [ "$#" -ne 1 ]
then
    usage
fi

CURR=`pwd`
MAINFOLDER=`basename $(dirname $CURR)`
FOLDER="$CURR/../../"

if [ -d "$FOLDER/DebianBuild" ]
then
  rm -rf $FOLDER/DebianBuild
  #echo "${FOLDER} Exists, Please Delete Path for Debian Creation"
  #exit 0
fi 	

mkdir "$FOLDER/DebianBuild"
cd $FOLDER/
FOLDERTAR=$MAINFOLDER-$1.tar.gz
tar -zcvf $FOLDERTAR $MAINFOLDER
mv $FOLDERTAR DebianBuild/
cd DebianBuild
tar -xzmf $FOLDERTAR
mv $MAINFOLDER $MAINFOLDER-$1
cd $MAINFOLDER-$1

debmake
cd debian
sed -i 's/Section: unknown/Section: embedded/' control


cd $MAINFOLDER-$1
debuild
#The deb package is present in DebianBuild Directory which is at the level of the repository 


