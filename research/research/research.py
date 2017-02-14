""" Setup Research Environments """
import os
import os.path
import argparse
from StringIO import StringIO
from fabric.api import run, sudo, put, cd, settings, task, execute


UBUNTU_PACKAGES = [
    'build-essential',
    'gdb',
    'clang',
    'cmake',
    'git',
    'python-dev',
    'python-pip',
    'htop',
    'atop',
    'sysstat',
    'emacs',
    'google-perftools',
    'libgoogle-perftools-dev',
]


def read_pubkey(pubkey_path):
    with open(pubkey_path, 'rb') as f:
        pubkey = f.read()
    return pubkey


@task
def install_packages():
    sudo('apt -y update')
    sudo('apt -y upgrade')
    sudo('apt -y install %s' % ' '.join(UBUNTU_PACKAGES))
    kernel_version = run('uname -r')
    sudo('apt -y install linux-tools-%s' % str(kernel_version))


@task
def install_authorized_key(newuser, pubkey):
    with cd('/home/%s' % newuser):
        sudo('mkdir -p .ssh')
        sudo('echo \'%s\' >> .ssh/authorized_keys' % pubkey)
        sudo('chown -R %s .ssh' % newuser)


@task
def create_remote_user(newuser):
    sudo('adduser --disabled-password %s' % newuser)
    sudo('usermod -a -G sudo %s' % newuser)
    put(local_path=StringIO('%s ALL=(ALL) NOPASSWD:ALL\n' % newuser),
        remote_path='/etc/sudoers.d/user-nopasswd-%s' % newuser,
        use_sudo=True)
    sudo('chown root:root /etc/sudoers.d/user-nopasswd-%s' % newuser)


def main_install_packages():
    ap = argparse.ArgumentParser()
    ap.add_argument('-H', dest='host', required=True)
    ap.add_argument('-U', dest='user', default='elliot')
    args = ap.parse_args()
    with settings(user=args.user, host_string=args.host):
        execute(install_packages)


def main_install_authorized_key():
    ap = argparse.ArgumentParser()
    ap.add_argument('-H', dest='host', required=True)
    ap.add_argument('-S', dest='superuser', default='ubuntu')
    ap.add_argument('-U', dest='user', default='elliot')
    ap.add_argument('--pubkey', default=os.path.join(os.environ['HOME'], '.ssh', 'id_rsa.pub'))
    args = ap.parse_args()
    pubkey = read_pubkey(args.pubkey)
    with settings(user=args.superuser, host_string=args.host):
        execute(install_authorized_key, args.user, pubkey)


def main_create_remote_user():
    ap = argparse.ArgumentParser()
    ap.add_argument('-H', dest='host', required=True)
    ap.add_argument('-S', dest='superuser', default='ubuntu')
    ap.add_argument('-U', dest='user', default='elliot')
    args = ap.parse_args()
    with settings(user=args.superuser, host_string=args.host):
        execute(create_remote_user, args.user)


def main_setup_user():
    ap = argparse.ArgumentParser()
    ap.add_argument('-H', dest='host', required=True)
    ap.add_argument('-S', dest='superuser', default='ubuntu')
    ap.add_argument('-U', dest='user', default='elliot')
    ap.add_argument('--pubkey', default=os.path.join(os.environ['HOME'], '.ssh', 'id_rsa.pub'))
    args = ap.parse_args()
    pubkey = read_pubkey(args.pubkey)
    with settings(user=args.superuser, host_string=args.host):
        execute(create_remote_user, args.user)
        execute(install_authorized_key, args.user, pubkey)
        execute(install_packages)
