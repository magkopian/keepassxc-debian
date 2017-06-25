KeePassXC Debian Package
================

This repository contains the necessary files for building Debian binary packages of [KeePassXC](https://keepassxc.org/).

## Building Dependencies

First make sure that all three `build-essential`, `debootstrap` and `devscripts`, `pbuilder` packages are installed on your system, as they contain the necessary tools for building the package.

```bash
sudo apt-get update
sudo apt-get install build-essential debootstrap devscripts pbuilder
```

After the installation is complete you will need to configure `pbuilder`. To do so, you will first need to set the `/var/cache/pbuilder/result` directory writable by your user account and then create the directory `/var/cache/pbuilder/hooks`, which again needs to be writable by your user.

```bash
sudo chown <user>:<user> /var/cache/pbuilder/result
sudo mkdir /var/cache/pbuilder/hooks
sudo chown <user>:<user> /var/cache/pbuilder/hooks
```

Next, using your favorite text editor add the following lines in either your `/etc/pbuilderrc` or `~/.pbuilderrc` after creating it.

```
AUTO_DEBSIGN=${AUTO_DEBSIGN:-no}
HOOKDIR=/var/cache/pbuilder/hooks
```

Finally, create a file named `B90lintian` inside your `/var/cache/pbuilder/hooks` directory with the following contents,

```bash
#!/bin/sh
set -e
install_packages() {
        apt-get -y --force-yes install "$@"
        }
install_packages lintian
echo "+++ lintian output +++"
su -c "lintian -i -I --show-overrides /tmp/buildd/*.changes" - pbuilder
# use this version if you don't want lintian to fail the build
#su -c "lintian -i -I --show-overrides /tmp/buildd/*.changes; :" - pbuilder
echo "+++ end of lintian output +++"
```

and make it executable,

```bash
chmod +x `/var/cache/pbuilder/hooks`
```

## Building the Package

First, obtain a fresh copy of the source package using `git clone`.

```bash
git clone https://github.com/magkopian/keepassxc-debian.git
```

Next, `cd` into the `keepassxc-debian` directory and using the `pbuilder` program create a `chroot` environment of the target Debian release (e.g. unstable) that you want to build the package for.

```bash
cd keepassxc-debian
sudo pbuilder create --distribution <debian-release>
sudo pbuilder --update --distribution <debian-release>
```

Finally, using the `dsc` file build the package in the `chroot` environment you just created.

```bash
sudo pbuilder --build keepassxc_<version>.dsc
```
The newly built package will be located inside the `/var/cache/pbuilder/result` directory, owned by your user account.

## Making Changes to the Package Sources

If you make changes to the package sources instead of using the previous `pbuilder` command for building the package, you will need to run `pdebuild` from the `keepassxc-<version>` directory.

```bash
cd keepassxc-<version>
pdebuild
```

The `pdebuild` program will generate the source package using the updated sources and also build the binary package for you.
