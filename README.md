# ISC Kea Hooks

## Goal

We need to store the correspondence between MAC address -> ipv4 address in the file system,
not in the configuration file.

Adding to classes based on "option82" without server config modification.

For this, a template with three parameters is used: the interface name ($i), subnet-id ($n) and the MAC address ($m).
The contents of the file (or symlink) is converted to the "flex-id" value using the inet_aton(); 
For each such value should be reserved.

Some managed switches can add "option82" to dhcp requests. We use D-Link swithes.

```
   Agent-Information Option 82, length 18:
     Circuit-ID SubOption 1, length 6: vlan 15 port 17
     Remote-ID SubOption 2, length 8: ID: "d8:fe:e3:6f:af:98"
hex dump:
     5212 0106 0004 000f 0011 0208 0006 d8fe e36f af98
```

This option specifies the vlan number ($v), the port number ($p), and the MAC address of the switch ($s).

These parameters are substituted into a wildcard string, which is the name of the file.
The client's MAC address ($c) can also be used in the template.

If there is such a file and a symbolic link, then their contents are considered as the name of the class.
Only letters, numbers and '\_' are allowed in class names.


Documentation for Kea and hooks:

- https://jenkins.isc.org/job/Kea_doc/doxygen/
- https://jenkins.isc.org/job/Kea_doc/doxygen/df/d46/hooksdgDevelopersGuide.html
- https://jenkins.isc.org/job/Kea_doc/doxygen/de/df3/dhcpv4Hooks.html

### Installation

You will first need to have installed [**ISC Kea**](https://www.isc.org/kea/) software **with** header/development packages (so the hook can link against it) - or use [**`addon-kea-hooks`**](https://github.com/OpenNebula/addon-kea-hooks) parent project (this very repo) and have all in one.

You can modify the build by editing the `Makefile.config` but there is not so many tweakable things to do except changing the name of the hook library, output directory and compiler flags (the names of the linked libraries might be in the need of fixing too - the ones used here are matching the installation on the Alpine Linux).

After that you simply run `make`:

```
% make
```

If everything went fine then you should see the compiled library in the `build` directory:

```
% ls -l build
```

And copy the library file into your Kea hook directory or just *install* it:

```
% make install
```

#### Hook parameters

- `debug` (`boolean`) - enable/disable the debug log [optional]
- `template_id` (`string`) - list of templates
- `template_class` (`string`) - list of templates
- `mode` (`string`) - all or first. first - stop after the first matched pattern, all - try all templates.
- `default_class` (`string`) - default class. Used if all templates does not match.

$m - client MAC-addess
$i - server interface name
$s - MAC-addess of switsh (opt82)
$v - vlan number (opt82)
$p - port number (opt82)
$n - subnet-id (only for id template!)


### Usage

```
"Dhcp4": {
    ...
    "interfaces-config": {
       "interfaces": [ "eth0/10.249.0.1" ],
       ...
    },
    "host-reservation-identifiers": [ "flex-id", ... ],
    ...
    "hooks-libraries": [
      {
        "library": "/opt/kea/lib/kea/hooks/libkea-flex-id.so",
        "parameters": {
            "debug": false,
            "template_id": "/tmpfs/flexid/$n/$m",
            "template_class": [ "/tmpfs/flexid/bad/$m", "/tmpfs/flexid/$s/$p" ],
	    "mode": "first",
            "default_class": "BAD"
        }
      }
      ...
    ],
    ...
    "subnet4": [
       "subnet": "10.249.0.0/24",
       "id": 100,
       "pools": [
           { "pool": "10.249.0.20 - 10.249.0.40",   "client-class": "L0P1" },
           { "pool": "10.249.0.41 - 10.249.0.60",   "client-class": "L0P2" },
           { "pool": "10.249.0.61 - 10.249.0.100",  "client-class": "BAD" }
       ],

       ...
       "reservations": [
             ....
             { "flex-id": "0x0A090102", "ip-address": "10.9.1.2" },
             { "flex-id": "0x00000003", "ip-address": "10.9.1.3" },
	     ...
       ]
    ]
}
```

cat '/tmpfs/flexid/100/00:01:02:03:03:AA'
```
10.9.1.2
```

cat '/tmpfs/flexid/100/00:01:02:03:03:BB'
```
0.0.0.3
```

cat '/tmpfs/flexid/00:01:02:03:03:AA/1'
```
L0P1
```

cat '/tmpfs/flexid/00:01:02:03:03:AA/2'
```
L0P2
```


Each "reserved" address needs an entry in the "reservations" section.
Flex-id is a hexadecimal value of four bytes.

The Flex-id plugin forms a file name according to the template specified
in the parameters and tries to read its contents. If the file is symlink,
then readlink() is executed.

The first line is read from the file, spaces at the beginning 
and end of the line are ignored. The resulting string is converted
to IPv4 address via inet_aton() and, if successful, the address is returned
as value of "flex-id".

