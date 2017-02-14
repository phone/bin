from setuptools import find_packages, setup

description = "Setup Research Environments"

setup(
    name='research-environments',
    version='0.0.1',
    author='Elliot Pennington',
    author_email='elliot@kilrog.com',
    description=description,
    packages=find_packages(exclude=['tests']),
    install_requires=[
        'fabric'
    ],
    entry_points={
        'console_scripts': [
            'research-setup-user = research.research:main_setup_user',
            'research-create-remote-user = research.research:main_create_remote_user',
            'research-install-authorized-key = research.research:main_install_authorized_key',
            'research-install-packages = research.research:main_install_packages',
        ],
    },
    include_package_data=True,
    platforms='any',
    zip_safe=False,
    license='MIT'
)
