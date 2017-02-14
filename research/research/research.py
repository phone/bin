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

PIP_PACKAGES = [
    'ipython',
    'ipdb',
    'supervisor',
    'virtualenv',
]

BASIS_REPOS = [
    'git@github.com:phone/bin.git',
    'git@github.com:phone/lib.git',
]


def read_pubkey(pubkey_path):
    """ Read a public key from a file and return the contents.

    Really, would work for any file. Its name is so specific here for readability elsewhere.

    Args:
        pubkey_path (str): Path to public key file.
    """
    with open(pubkey_path, 'rb') as f:
        pubkey = f.read()
    return pubkey


@task
def prepare_user(remote_keyfile='~/.ssh/id_rsa', repos=BASIS_REPOS):
    """ Checkout my bin and lib directories

    Args:
        remote_keyfile (str): The keyfile on the remote host to use when checking out repos.

    Warnings:
        Assumes that a keyfile is already generated and installed on the git remote origin (typically Github).
    """
    home = run('echo $HOME')
    with cd(str(home)):
        run('mkdir -p src')
        for repo in repos:
            run("ssh-agent bash -c 'ssh-add %s; git clone %s'" % (remote_keyfile, repo), shell=True)


@task
def install_packages():
    sudo('apt -y update')
    sudo('apt -y upgrade')
    sudo('apt -y install %s' % ' '.join(UBUNTU_PACKAGES))
    kernel_version = run('uname -r')
    sudo('apt -y install linux-tools-%s' % str(kernel_version))
    run('pip install --upgrade pip')
    run('pip install %s' % ' '.join(PIP_PACKAGES))


@task
def install_authorized_key(newuser, pubkey):
    """ Install a public key into remote .ssh/authorized_keys

    This is just like ssh-copy-id, and exists here only so we can call it as part of a fabric pipeline.

    Notes:
        Assumes we log in as a different superuser.

    Args:
        newuser (str): The user into whos HOME we're putting this
        pubkey (str): The text of the public key to add.
    """
    with cd('/home/%s' % newuser):
        sudo('mkdir -p .ssh')
        sudo('echo \'%s\' >> .ssh/authorized_keys' % pubkey)
        sudo('chown -R %s .ssh' % newuser)


@task
def create_remote_user(newuser):
    """ Create a user on a remote machine.

    Warnings:
        This seems to have some interactive prompts that I haven't figured out how to eliminate.

    Args:
        newuser (str): The username of the user to add.
    """
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


def main_prepare_user():
    ap = argparse.ArgumentParser()
    ap.add_argument('-H', dest='host', required=True)
    ap.add_argument('-U', dest='user', default='elliot')
    ap.add_argument('--remote-private-key', default='~/.ssh/id_rsa')
    ap.add_argument('--basis-repos', default=','.join(BASIS_REPOS))
    args = ap.parse_args()
    with settings(user=args.user, host_string=args.host):
        execute(prepare_user, args.remote_private_key, args.basis_repos.split(','))
