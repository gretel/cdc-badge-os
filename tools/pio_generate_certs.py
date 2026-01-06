Import("env")

from pathlib import Path
import subprocess


def _generate_embedded_cert(source, target, env_):
    project_dir = Path(env_.subst("$PROJECT_DIR"))
    build_dir = Path(env_.subst("$BUILD_DIR"))
    build_dir.mkdir(parents=True, exist_ok=True)

    idf_path = Path(env_.PioPlatform().get_package_dir("framework-espidf"))
    cmake_path = Path(env_.PioPlatform().get_package_dir("tool-cmake")) / "bin" / "cmake"
    embed_script = idf_path / "tools" / "cmake" / "scripts" / "data_file_embed_asm.cmake"

    for cert_path in project_dir.glob("managed_components/**/server_certs/*.crt"):
        out_path = build_dir / f"{cert_path.name}.S"
        subprocess.check_call(
            [
                str(cmake_path),
                f"-D",
                f"DATA_FILE={cert_path}",
                f"-D",
                f"SOURCE_FILE={out_path}",
                f"-D",
                "FILE_TYPE=TEXT",
                "-P",
                str(embed_script),
            ],
            cwd=str(build_dir),
        )


_generate_embedded_cert(None, None, env)
