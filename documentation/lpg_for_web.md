# LPG for web

## Usage
To generate a HTML page out of this LPG code, you run the LPG compiler in the web mode with `lpg web <LPG FILE> <OUTPUT FILE>`. This will generate an simple HTML structure with an empty body and the script tag in the head. 

### Customizing the template
If you want to have customize the template just create a HTML file with the same name (eg. `web.lpg` and `web.html`). In this file the text `|LPG|` will be replaced with the generated Javascript.

## Code examples

If you want to write a web applcation with LPG that compiles to Ecmascript then you can use the `ecmascript` module in LPG.

A simple "Hello world" application would look like this:
```lpg
let std = import std
let es = import ecmascript
let option = std.option

(window: host_value, host: es.host)
    let arguments = new_array(host_value)
    assert(arguments.append(host.export-string("Hello world!")))
    match host.read-property(window, "document")
        case option[host_value].some(let console):
            host.call-method(console, "write", arguments)
            std.unit_value
        case option[host_value].none:
            assert(boolean.false)
            std.unit_value
```