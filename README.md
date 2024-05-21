# bob

bob is a **B**inary **O**nly package manager that is for managing my publicly published softwares. Sample repository can be found on [here](https://github.com/night0721/bob-packages), only works on musl-libc system.

It is recommended to have `XDG_DATA_HOME` defined to store the database for bob, otherwise database file would be created at `~/.cache`

# Customizing

You may use your own Github repository for supplying the binaries, or other platforms but requiring you to modify the source to use other URL to download.

All these customizations can be done in `bob.h`

# Usage
```
Usage:
 (install|i|add) <package>	    Install a package
 (uninstall|d|del) <package>	Uninstall a package
 (update|u) <package>		    Update a package
 (search|s) <package>		    Search for a package
 (list|l) [all] 		        List all packages installed/available
```

# Dependencies

- gcc
- curl

# Building

You will need to run these with elevated privilages.

```sh
$ make 
# make install
```

# Contributions
Contributions are welcomed, feel free to open a pull request.

# License
This project is licensed under the GNU Public License v3.0. See [LICENSE](https://github.com/night0721/bob/blob/master/LICENSE) for more information.
