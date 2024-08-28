from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

extensions = [Extension('wrapper',
                        ['wrapper.pyx'],
                        language="c",
                        extra_compile_args=["-std=gnu11", "-fopenmp", "-O3"],
                        extra_link_args=["-lz", "-lm", "-lgomp"]
                    )]
setup(
    name='wrapper', 
    ext_modules=cythonize(extensions)
)