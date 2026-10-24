# noteman

> THIS PROJECT IS NOT YET COMPLETE

```text
███╗   ██╗ ██████╗ ████████╗███████╗███╗   ███╗ █████╗ ███╗   ██╗
████╗  ██║██╔═══██╗╚══██╔══╝██╔════╝████╗ ████║██╔══██╗████╗  ██║
██╔██╗ ██║██║   ██║   ██║   █████╗  ██╔████╔██║███████║██╔██╗ ██║
██║╚██╗██║██║   ██║   ██║   ██╔══╝  ██║╚██╔╝██║██╔══██║██║╚██╗██║
██║ ╚████║╚██████╔╝   ██║   ███████╗██║ ╚═╝ ██║██║  ██║██║ ╚████║
╚═╝  ╚═══╝ ╚═════╝    ╚═╝   ╚══════╝╚═╝     ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝
                                                                 
```

> A local notebook manager, manage your class materials based on course and session, provide you an Integrated-Studying-Environment with GUI. Your files with be stored strictly to where it belongs to automatically, and you can do most stuffs you need to do in a session without context-switching.

---

## Install

Visit the [release](https://github.com/tungle2202/noteman/releases) tab
to install prebuild binary package or install wizard

## Prerequisites

To build Noteman from source, install the [core development toolchains](#1-core-toolchains-all-platforms) and
[platform-specific dependencies](#2-platform-dependencies) for your operating system.

### 1. Core Toolchains (All Platforms)

- **Rust**: Version 1.77+ (2021 edition) installed via [rustup](https://rustup.rs/):

```zsh
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

- **Node.js**: Version 18+ or 20+ LTS with `npm` (download from [nodejs.org](https://nodejs.org/)).

---

### 2. Platform Dependencies

<details open>
<summary><b>Linux</b></summary>

Linux requires GTK, WebKitGTK, and C build tools to compile Noteman.

**Recommended backend:** GTK 4 + WebKitGTK 6.0 for modern WebKit features and
improved native integration.

**Compatibility backend:** GTK 3 + WebKit2GTK 4.1 is also supported where
required by the application or its dependencies. Some features may behave
differently depending on the WebKitGTK version.

#### Debian / Ubuntu (`apt`)

**Recommended: GTK 4 + WebKitGTK 6.0**

```zsh
sudo apt update
sudo apt install -y \
    build-essential \
    curl \
    wget \
    file \
    pkg-config \
    libssl-dev \
    libgtk-4-dev \
    libwebkitgtk-6.0-dev \
    libayatana-appindicator3-dev \
    librsvg2-dev
```

**Alternative: GTK 3 + WebKit2GTK 4.1**

```zsh
sudo apt update
sudo apt install -y \
    build-essential \
    curl \
    wget \
    file \
    pkg-config \
    libssl-dev \
    libgtk-3-dev \
    libwebkit2gtk-4.1-dev \
    libayatana-appindicator3-dev \
    librsvg2-dev
```

#### Arch Linux (`pacman`)

**Recommended: GTK 4 + WebKitGTK 6.0**

```zsh
sudo pacman -Syu --needed \
    base-devel \
    curl \
    wget \
    file \
    pkgconf \
    openssl \
    gtk4 \
    webkitgtk-6.0 \
    libayatana-appindicator \
    librsvg
```

**Alternative: GTK 3 + WebKit2GTK 4.1**

```zsh
sudo pacman -Syu --needed \
    base-devel \
    curl \
    wget \
    file \
    pkgconf \
    openssl \
    gtk3 \
    webkit2gtk-4.1 \
    libayatana-appindicator \
    librsvg
```

#### Fedora / Red Hat (`dnf`)

**Recommended: GTK 4 + WebKitGTK 6.0**

```zsh
sudo dnf group install -y "Development Tools"
sudo dnf install -y \
    curl \
    wget \
    file \
    pkgconf-pkg-config \
    openssl-devel \
    gtk4-devel \
    webkitgtk6.0-devel \
    libayatana-appindicator-gtk3-devel \
    librsvg2-devel
```

**Alternative: GTK 3 + WebKit2GTK 4.1**

```zsh
sudo dnf group install -y "Development Tools"
sudo dnf install -y \
    curl \
    wget \
    file \
    pkgconf-pkg-config \
    openssl-devel \
    gtk3-devel \
    webkit2gtk4.1-devel \
    libayatana-appindicator-gtk3-devel \
    librsvg2-devel
```

**Note:** Package availability depends on the distribution release, enabled repositories, and CPU architecture. Verify that the required development packages are available before building.

GTK 3 remains a compatibility option. GTK 4 + WebKitGTK 6.0 is preferred for modern WebKit features, provided the selected Tauri backend and its dependencies support it.

</details>

<details>
<summary><b>macOS</b></summary>

macOS uses Apple's native WebKit through `WKWebView`. GTK and WebKitGTK are not required.

1. **Install Apple Command Line Tools** (Clang, Make, Git, and Apple SDKs):

   ```zsh
   xcode-select --install
   ```

2. **Install Node.js** using [Homebrew](https://brew.sh/):

   ```zsh
   brew install node
   ```

</details>

<details>
<summary><b>Windows</b></summary>

Windows uses Microsoft WebView2 for the native webview.

- **Microsoft C++ Build Tools:** Install [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) and select **Desktop development with C++**.
- **WebView2 Runtime:** Install the [Microsoft Edge WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) if it is not already available.

</details>
---

## Build guide

### 1. Install Node Dependencies

```zsh
npm install
```

### 2. Run in Development Mode

Starts Vite dev server and launches the native desktop window with live reload:

```zsh
npm run tauri dev
```

### 3. Build for Production

Compiles the optimized Rust backend and packages the application binary:

```zsh
npm run tauri build
```

The output installers/executables will be located in `src-tauri/target/release/bundle/`.
