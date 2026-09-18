# Code signing / Smart App Control

MPV WinterStatic Edition can produce an Authenticode-signed portable build.
Signing is performed **after** the libmpv runtime is collected, so the finished
portable binaries are the files that are signed and verified.

## What this solves

Windows 11 Smart App Control can block unknown unsigned code. Microsoft advises
developers to sign application binaries with an RSA code-signing certificate
issued by a trusted provider. A self-signed certificate is useful for private
lab/testing scenarios only when you explicitly trust that certificate yourself;
it does not make a public download generally trusted by Smart App Control.

The build signs the frontend EXE and any packaged EXE/DLL files that do not
already have a valid Authenticode signature. Existing valid third-party
signatures are preserved. Signing a bundled third-party DLL is an attestation of
the exact binary shipped in the WinterStatic package; it does not change that
component's authorship or license. Runtime provenance and licenses remain under
`libmpv\`.

## One-time setup

1. Obtain an **RSA** code-signing certificate from a trusted provider. This
   patch supports certificates exposed through the Windows certificate store or
   a PFX/P12 file. Microsoft Trusted Signing uses a different SignTool integration
   and can be added later if that is the certificate route you choose.
2. Copy:

       signing.local.cmd.example

   to:

       signing.local.cmd

3. Configure either:
   - `WINTERSTATIC_SIGN_THUMBPRINT` for a certificate available in the Windows
     certificate store, or
   - `WINTERSTATIC_SIGN_PFX` for a PFX/P12 certificate file.
4. If the PFX is password-protected, preferably set its password only for the
   current terminal session before the build:

       set "WINTERSTATIC_SIGN_PFX_PASSWORD=your-password"

`signing.local.cmd` is excluded by `.gitignore`.

## Build commands

Normal unsigned development build:

    build-native.bat

Signed portable build (no runtime-source download):

    build-native.bat signed

Normal public-release build with matching runtime source:

    build-native.bat release

Signed public-release build with matching runtime source:

    build-native.bat signed-release

Signed modes are strict. If SignTool, the certificate, timestamp service, or
signature verification fails, the build reports **CODE SIGNING FAILED** and does
not silently fall back to calling the package signed.

## GitHub releases

GitHub does not automatically Authenticode-sign uploaded release assets. To avoid
Smart App Control blocking an unknown unsigned frontend, create the public binary
with `build-native.bat signed-release`, verify the signature, then upload that
signed portable package to the GitHub Release. Keep certificate private keys and
PFX/P12 files out of the repository.

## Verification

The build automatically runs SignTool verification after each newly signed file.
You can also verify the frontend manually from a Visual Studio Developer Command
Prompt:

    signtool verify /pa /v "MPV-WinterStatic-Edition.exe"

Windows Explorer also shows a **Digital Signatures** tab in the file properties
for an Authenticode-signed executable.
