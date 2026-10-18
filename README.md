# noteman

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

To build Noteman from source, install the core development toolchains and
platform-specific dependencies for your operating system.

### 1. Core Toolchains (All Platforms)

- **Rust**: Version 1.77+ (2021 edition) installed via [rustup](https://rustup.rs/):

```zsh
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

- **Node.js**: Version 18+ or 20+ LTS with `npm` (download from [nodejs.org](https://nodejs.org/)).

---

### 2. Platform Dependencies

<details open>
<summary><b>linux</b></summary>

Linux requires GTK 3, WebKitGTK 4.1, and C build tools to compile:

#### Debian Family (`apt`)

```zsh
sudo apt update && sudo apt install -y \
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

#### Arch Linux Family (`pacman`)

```zsh
sudo pacman -Syu --needed \
    base-devel \
    curl \
    wget \
    pkgconf \
    openssl \
    gtk3 \
    webkit2gtk-4.1 \
    libayatana-appindicator \
    librsvg
```

#### Red Hat Family (`dnf`)

```zsh
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y \
    curl \
    wget \
    pkgconf-pkg-config \
    openssl-devel \
    gtk3-devel \
    webkit2gtk4.1-devel \
    libayatana-appindicator-gtk3-devel \
    librsvg2-devel
```

</details>

<details>
<summary><b>macOS</b></summary>

macOS uses built-in Apple WebKit (`WKWebView`) and does not require GTK or external webview libraries.

1. **Install Apple Command Line Tools** (Clang, Make, Git, Apple SDKs):

   ```zsh
   xcode-select --install
   ```

2. **Install Node.js** (via [Homebrew](https://brew.sh/)):

   ```zsh
   brew install node
   ```

</details>

<details>
<summary><b>Windows</b></summary>

- [Microsoft C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) (select "Desktop development with C++").
- [WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) (pre-installed on Windows 11).

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
