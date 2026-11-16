{
  description = "SelfGuitar development environment and CMake checks";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

    nixGL = {
      url = "github:nix-community/nixGL";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, nixGL, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      llvm = pkgs.llvmPackages_18;
      qt = pkgs.qt6;

      mkShell = pkgs.mkShell.override {
        stdenv = llvm.stdenv;
      };

      mkQualityTool = tool: pkgs.writeShellScriptBin "${tool}-18" ''
        exec ${llvm.clang-tools}/bin/${tool} "$@"
      '';

      qualityTools = map mkQualityTool [ "clang-format" "clang-tidy" ];
    in
    {
      devShells.${system}.default = mkShell {
        packages = [
          pkgs.cmake
          pkgs.ninja
          pkgs.git
          pkgs.python3
          llvm.clang-tools
          nixGL.packages.${system}.nixGLIntel
        ] ++ qualityTools;

        buildInputs = [ qt.qtbase qt.qtdeclarative qt.qtserialport ];

        shellHook = ''
          export QT_PLUGIN_PATH="${qt.qtbase}/lib/qt-6/plugins"
          export QML_IMPORT_PATH="${qt.qtdeclarative}/lib/qt-6/qml"
          echo "SelfGuitar environment: LLVM 18, CMake, Ninja and Qt."
        '';
      };

      checks.${system} = {
        protocol = pkgs.runCommand "selfguitar-protocol-tests" {
          nativeBuildInputs = [ pkgs.cmake pkgs.ninja llvm.stdenv.cc ];
        } ''
          cmake -S ${self} -B build -G Ninja \
            -DGUITAR_BUILD_APP=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_COMPILER=${llvm.stdenv.cc}/bin/clang++
          cmake --build build
          ctest --test-dir build --output-on-failure --no-tests=error
          touch "$out"
        '';

        application = pkgs.runCommand "selfguitar-application" {
          nativeBuildInputs = [ pkgs.cmake pkgs.ninja llvm.stdenv.cc ];
          buildInputs = [ qt.qtbase qt.qtdeclarative qt.qtserialport ];
        } ''
          cmake -S ${self} -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_COMPILER=${llvm.stdenv.cc}/bin/clang++
          cmake --build build
          ctest --test-dir build --output-on-failure --no-tests=error
          export HOME="$TMPDIR/home"
          export XDG_CACHE_HOME="$HOME/.cache"
          mkdir -p "$XDG_CACHE_HOME"
          export FONTCONFIG_FILE=${pkgs.makeFontsConf { fontDirectories = [ pkgs.dejavu_fonts ]; }}
          export QT_PLUGIN_PATH="${qt.qtbase}/lib/qt-6/plugins"
          export QML_IMPORT_PATH="${qt.qtdeclarative}/lib/qt-6/qml"
          QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
            build/bin/SelfGuitar --screenshot "$TMPDIR/preview.png"
          touch "$out"
        '';
      };
    };
}
