#!/bin/bash
set -e

VERSION="${1:-1.0.0}"
ARCH="${2:-aarch64}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

echo "Building termux-midi ${VERSION} for ${ARCH}"

# Update version in control files
sed -i "s/^Version: .*/Version: ${VERSION}/" packaging/termux-midi.control
sed -i "s/^Architecture: .*/Architecture: ${ARCH}/" packaging/termux-midi.control
sed -i "s/^Version: .*/Version: ${VERSION}/" packaging/termux-midi-soundfont.control

# Clean up any previous builds
rm -rf pkg sfpkg
rm -f termux-midi_*.deb termux-midi-soundfont_*.deb

# Verify binary exists
if [ ! -f "termux-midi" ]; then
    echo "Error: termux-midi binary not found. Run 'make strip' first."
    exit 1
fi

# Create termux-midi package
echo "Creating termux-midi package..."
mkdir -p pkg/data/data/com.termux/files/usr/bin
cp termux-midi pkg/data/data/com.termux/files/usr/bin/

# Use termux-create-package if available, otherwise create manually
if command -v termux-create-package &> /dev/null; then
    termux-create-package packaging/termux-midi.control pkg/
else
    echo "Creating .deb package manually..."
    mkdir -p pkg/DEBIAN
    cp packaging/termux-midi.control pkg/DEBIAN/control

    # Create data.tar.xz
    (cd pkg/data && tar -cJf ../data.tar.xz .)

    # Create control.tar.xz
    (cd pkg/DEBIAN && tar -cJf ../control.tar.xz .)

    # Create debian-binary
    echo "2.0" > pkg/debian-binary

    # Create the .deb
    ar rcs "termux-midi_${VERSION}_${ARCH}.deb" pkg/debian-binary pkg/control.tar.xz pkg/data.tar.xz

    # Cleanup
    rm -f pkg/debian-binary pkg/control.tar.xz pkg/data.tar.xz
fi

# Create soundfont package
echo "Creating termux-midi-soundfont package..."
mkdir -p sfpkg/data/data/com.termux/files/usr/share/soundfonts
cp soundfonts/default.sf3 sfpkg/data/data/com.termux/files/usr/share/soundfonts/

if command -v termux-create-package &> /dev/null; then
    termux-create-package packaging/termux-midi-soundfont.control sfpkg/
else
    echo "Creating soundfont .deb package manually..."
    mkdir -p sfpkg/DEBIAN
    cp packaging/termux-midi-soundfont.control sfpkg/DEBIAN/control

    # Create data.tar.xz
    (cd sfpkg/data && tar -cJf ../data.tar.xz .)

    # Create control.tar.xz
    (cd sfpkg/DEBIAN && tar -cJf ../control.tar.xz .)

    # Create debian-binary
    echo "2.0" > sfpkg/debian-binary

    # Create the .deb
    ar rcs "termux-midi-soundfont_${VERSION}_all.deb" sfpkg/debian-binary sfpkg/control.tar.xz sfpkg/data.tar.xz

    # Cleanup
    rm -f sfpkg/debian-binary sfpkg/control.tar.xz sfpkg/data.tar.xz
fi

# Cleanup temp directories
rm -rf pkg sfpkg

echo ""
echo "Packages created:"
ls -la termux-midi*.deb

echo ""
echo "To install in Termux:"
echo "  dpkg -i termux-midi_${VERSION}_${ARCH}.deb"
echo "  dpkg -i termux-midi-soundfont_${VERSION}_all.deb"
