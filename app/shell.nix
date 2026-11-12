let
  source = builtins.fetchTarball {
    url = "https://releases.nixos.org/nixpkgs/nixpkgs-26.11pre1117836.9397dcb09c95/nixexprs.tar.zst";
    sha256 = "sha256-jnNhqdXGJs5qIvdzFwABBvhzuPHLK09CcWKtvYVxVOk=";
  };
  pkgs = import source {};
in pkgs.mkShell {
  nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.pkg-config ];
  buildInputs = [ pkgs.qt6.qtbase pkgs.qt6.qtdeclarative pkgs.qt6.qtserialport ];
  shellHook = ''
    export NIX_PATH="nixpkgs=${source}"
    export QT_PLUGIN_PATH="${pkgs.qt6.qtbase}/lib/qt-6/plugins"
    export QML_IMPORT_PATH="${pkgs.qt6.qtdeclarative}/lib/qt-6/qml"
  '';
}
