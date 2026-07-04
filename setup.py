#!/usr/bin/env python3
"""
setup.py for geom-core

Builds the C++ extension using CMake and pybind11.
This allows users to install via: pip install .
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    """Custom Extension class for CMake-based builds"""
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    """Custom build_ext command that invokes CMake"""

    def run(self):
        # Check if CMake is available
        try:
            subprocess.check_output(['cmake', '--version'])
        except OSError:
            raise RuntimeError(
                "CMake must be installed to build the following extensions: " +
                ", ".join(e.name for e in self.extensions))

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        # Ensure the build directory exists
        build_temp = Path(self.build_temp)
        build_temp.mkdir(parents=True, exist_ok=True)

        cmake_args = [
            f'-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}',
            f'-DPYTHON_EXECUTABLE={sys.executable}',
            '-DBUILD_PYTHON_BINDINGS=ON',
            '-DBUILD_WASM_BINDINGS=OFF',
        ]

        # Configuration (Release by default)
        cfg = os.environ.get('CMAKE_BUILD_TYPE', 'Release')
        build_args = ['--config', cfg]

        # Platform-specific settings
        if sys.platform.startswith('darwin'):
            # macOS specific
            cmake_args += [
                '-DCMAKE_OSX_DEPLOYMENT_TARGET=10.14',
            ]

        cmake_args += [f'-DCMAKE_BUILD_TYPE={cfg}']

        # Add parallel build flag
        if sys.platform.startswith('linux') or sys.platform.startswith('darwin'):
            # Use all available cores
            import multiprocessing
            build_args += ['--', f'-j{multiprocessing.cpu_count()}']
        elif sys.platform.startswith('win'):
            build_args += ['--', '/m']

        env = os.environ.copy()
        env['CXXFLAGS'] = f"{env.get('CXXFLAGS', '')} -DVERSION_INFO=\\\"{self.distribution.get_version()}\\\""

        # Run CMake configure
        subprocess.check_call(
            ['cmake', ext.sourcedir] + cmake_args,
            cwd=self.build_temp,
            env=env
        )

        # Run CMake build
        subprocess.check_call(
            ['cmake', '--build', '.'] + build_args,
            cwd=self.build_temp
        )

        # Copy the built extension to the package directory
        # The CMake build outputs to build/python/
        built_lib = None
        python_dir = Path(self.build_temp) / 'python'

        if python_dir.exists():
            # Find the built .so/.pyd file
            for pattern in ['*.so', '*.pyd', '*.dylib']:
                libs = list(python_dir.glob(pattern))
                if libs:
                    built_lib = libs[0]
                    break

        if built_lib and built_lib.exists():
            # Copy to the package directory
            dest_dir = Path(extdir)
            dest_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(str(built_lib), str(dest_dir))
            print(f"Copied {built_lib} to {dest_dir}")
        else:
            print(f"Warning: Could not find built library in {python_dir}")


# Read the README for long description
readme_path = Path(__file__).parent / 'README.md'
long_description = ""
if readme_path.exists():
    long_description = readme_path.read_text(encoding='utf-8')

setup(
    name='geom-core',
    version='0.1.0',
    author='MadFam',
    author_email='contact@madfam.io',
    description='High-performance C++ geometry analysis engine for 3D printing',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://github.com/madfam-io/geom-core',
    ext_modules=[CMakeExtension('geom_core_py')],
    cmdclass=dict(build_ext=CMakeBuild),
    zip_safe=False,
    python_requires='>=3.7',
    install_requires=[],
    extras_require={
        'dev': ['pytest', 'pytest-cov', 'black', 'flake8'],
    },
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'Topic :: Scientific/Engineering :: Mathematics',
        'License :: OSI Approved :: Apache Software License',
        'Programming Language :: Python :: 3',
        'Programming Language :: C++',
    ],
)
