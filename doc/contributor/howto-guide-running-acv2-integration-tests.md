# How-to Guide: running ACv2 integration tests

You need a VM in a special network (this may change over time):

```shell
gcloud compute networks create fastbyte \
    --project=coryan-test --subnet-mode=custom --mtu=8896 --bgp-routing-mode=regional
gcloud compute networks subnets create fastbyte-dual \
    --project=coryan-test --description="Fastbyte network with dual IPv4 and IPv6 support" \
    --range=10.0.0.0/24 --stack-type=IPV4_IPV6 \
    --ipv6-access-type=EXTERNAL --network=fastbyte \
    --region=us-central1 --enable-private-ip-google-access
```

Create a VM using this network:

```shell
gcloud compute instances create fastbyte-01 \
    --project=coryan-test --zone=us-central1-a --machine-type=n2d-standard-32 \
    --network-interface=ipv6-network-tier=PREMIUM,network-tier=PREMIUM,nic-type=GVNIC,stack-type=IPV4_IPV6,subnet=fastbyte-dual \
    --maintenance-policy=MIGRATE --provisioning-model=STANDARD \
    --service-account=558762729083-compute@developer.gserviceaccount.com \
    --scopes=https://www.googleapis.com/auth/cloud-platform \
    --create-disk=auto-delete=yes,boot=yes,device-name=fastbyte-01,image=projects/debian-cloud/global/images/debian-12-bookworm-v20240611,mode=rw,size=2048,type=projects/coryan-test/zones/us-central1-a/diskTypes/pd-balanced --no-shielded-secure-boot \
    --shielded-vtpm --shielded-integrity-monitoring --labels=goog-ec-src=vm_add-gcloud --reservation-affinity=any
```
