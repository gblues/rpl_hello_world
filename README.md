# rpl_hello_world

The goal here is to demonstrate how to dynamically load a home-made RPL and
invoke it.

- libhello/ is the library
- hello_world_demo.c is the RPX source

This demo currently builds, but doesn't work.

## Building

### Setup

For best results, use the devcontainers feature of VSCode. Further instructions
will assume you're using this setup.

- Install Docker and VSCode
- Install the 'Remote - Containers' extension
- Click the green `><` icon in the bottom-left corner and choose 'Reopen in container'
  (This will take a few minutes the first time as it builds the container)
- press CTRL+\` to open a shell inside the container

### Generating a build

```shell
cd /build
mkdir libhello demo
cd libhello
wiiu-cmake /workspaces/rpl_hello_world/libhello/
make
../demo 
wiiu-cmake /workspaces/rpl_hello_world/
make
cd /workspaces/rpl_hello_world
cp /build/libhello/libhello.rpl demo/
cp /build/demo/hello_world.rpx demo/
```
The `demo/` folder can then be copied to `/wiiu/apps` on your SD card.

... or, it could, if it actually worked.


