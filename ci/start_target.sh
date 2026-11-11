#!/usr/bin/env bash
# Démarre la « VM seed » : conteneur Ubuntu avec systemd, configuré comme l'image SEED
# de la plateforme, dans le pire cas pour l'anti-triche :
#   - ubuntu : compte de gestion (cloud-init), sudo sans mot de passe ;
#   - seed / dees : compte des étudiants, administrateur (sudo, adm, docker, lxd,
#     règle NOPASSWD) avec une session déjà ouverte.
set -euo pipefail
IMAGE="$1"

docker run -d --name seed --hostname seed --privileged --cgroupns=host \
  -v /sys/fs/cgroup:/sys/fs/cgroup:rw "$IMAGE" > /dev/null

state=""
for _ in $(seq 1 60); do
  state=$(docker exec seed systemctl is-system-running 2> /dev/null || true)
  if [ "$state" = running ] || [ "$state" = degraded ]; then break; fi
  sleep 2
done
echo "systemd : ${state:-inconnu}"

docker exec seed bash -c '
  set -e
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -qq
  apt-get install -y -qq openssh-server sudo python3 poppler-utils > /dev/null
  id ubuntu > /dev/null 2>&1 || useradd -m -s /bin/bash ubuntu
  echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/90-cloud-init-users
  chmod 440 /etc/sudoers.d/90-cloud-init-users
  groupadd -f docker
  groupadd -f lxd
  useradd -m -s /bin/bash -G sudo,adm,docker,lxd seed
  echo "seed:dees" | chpasswd
  echo "seed ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/seed
  chmod 440 /etc/sudoers.d/seed'
docker exec -d -u seed seed sleep 100000      # session « ouverte avant le provisioning »

docker exec seed bash -c '. /etc/os-release; echo "Cible : $PRETTY_NAME, $(python3 -V 2>&1), noyau $(uname -r)"'
docker exec seed bash -c 'echo "Avant provisioning : seed -> $(id -nG seed)"; sudo -l -U seed | tail -1'
