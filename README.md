Silent Hill Play Novel Translation
==================================

### Installation

Copy the BPS/IPS patch release for your chosen language and apply it to the original game ROM.

### Building the patches

Run `make help` in top level directory to learn about build configuration and supported targets.

Run `./build/shpn_tool` to learn about tool usage.

Project structure:

- `src`: tool source code
- `agb`: GBA ROM code patches
- `scripts`: translation scripts
- `test`: tool tests

### Adding a new translation

The preferred approach is to simply copy and modify an existing translation:

- `Harry` or `Cybil` are the respective character scenarios, obtained from script disassembly.
The strings present in the script can be translated directly. Additionally, the script itself may
be modified if needed. Note that `shpn_tool` performs automated text wrapping when embedding a
script, so the long lines should not be split manually unless non-standard layout is needed.
- `strtab_menu` and `strtab_script` contain additional strings that are not referenced directly
in the scripts.

Historically, the first translation was bootstrapped as follows:

- Embed the fully translated strtabs.
- Dump the resulting scripts.

### Debugging script execution

Obtain the target command address ```ADDR``` by dumping the script and inspecting the generated
commentary. Play until a HandleInput, connect via gdb, and execute

```
 p *(int*)0x3002070=ADDR
```

to set the next command address to ```ADDR```.

### Copyright & Acknowledgements

See script/ACKNOWLEDGEMENTS.
