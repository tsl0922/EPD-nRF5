#!/bin/sh
# Build/publish EpdHub inside the .NET 8 SDK container (run from WSL or any docker host).
# Usage: ./build.sh            -> publish to windows/publish/
#        ./build.sh build      -> compile only (faster, for checking errors)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
MODE="${1:-publish}"
NUGET_VOL="epdhub-nuget"

docker volume create "$NUGET_VOL" >/dev/null

if [ "$MODE" = "build" ]; then
  docker run --rm \
    -v "$HERE:/src" -v "$NUGET_VOL:/root/.nuget/packages" -w /src/EpdHub \
    mcr.microsoft.com/dotnet/sdk:8.0 \
    dotnet build -c Release -nologo
else
  docker run --rm \
    -v "$HERE:/src" -v "$NUGET_VOL:/root/.nuget/packages" -w /src/EpdHub \
    mcr.microsoft.com/dotnet/sdk:8.0 \
    dotnet publish -c Release -nologo -o /src/publish
  echo "published to $HERE/publish (run publish/EpdHub.exe on Windows)"
fi
