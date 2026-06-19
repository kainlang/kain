from setuptools import Extension, setup


setup(
    ext_modules=[
        Extension(
            "pykain._native",
            sources=["pykain/_native.c"],
        )
    ],
)
