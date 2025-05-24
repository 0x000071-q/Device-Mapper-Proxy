#include <linux/device-mapper.h>

static struct dmp_stats {
  atomic64_t count_read;
  atomic64_t count_write;
  atomic64_t size_block_write;
  atomic64_t size_block_read;
} dmp_stats;

static ssize_t volumes_show(struct kobject *kobj, struct kobj_attribute *attr,
                            char *buffer) {
  u64 count_read = atomic64_read(&dmp_stats.count_read);
  u64 count_write = atomic64_read(&dmp_stats.count_write);
  u64 sb_read = atomic64_read(&dmp_stats.size_block_read);
  u64 sb_write = atomic64_read(&dmp_stats.size_block_write);

  u64 avg_read = count_read ? sb_read / count_read : 0;
  u64 avg_write = count_write ? sb_write / count_write : 0;
  u64 total_reqs = count_read + count_write;
  u64 avg_total = total_reqs ? (sb_read + sb_write) / total_reqs : 0;

  return scnprintf(buffer, PAGE_SIZE,
                   "Read:\n  reqs: %llu\n  avg size: %llu bytes\n"
                   "Write:\n  reqs: %llu\n  avg size: %llu bytes\n"
                   "Total:\n  reqs: %llu\n  avg size: %llu bytes\n",
                   count_read, avg_read, count_write, avg_write, total_reqs,
                   avg_total);
}

struct dmp_target {
  struct dm_dev *dev;
};

static struct kobject *dmp_kobj;

static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
  struct dmp_target *dt = kzalloc(sizeof(*dt), GFP_KERNEL);

  if (argc < 1) {
    ti->error = "Not enough arguments";
    return -EINVAL;
  }

  if (!dt) {
    ti->error = "Cannot allocate dmp context";
    return -ENOMEM;
  }

  int err = dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &dt->dev);
  if (err) {
    ti->error = "Device lookup failed";
    kfree(dt);
    return err;
  }

  ti->private = dt;

  atomic64_set(&dmp_stats.count_read, 0);
  atomic64_set(&dmp_stats.count_write, 0);
  atomic64_set(&dmp_stats.size_block_read, 0);
  atomic64_set(&dmp_stats.size_block_write, 0);

  return 0;
}

static void dmp_dtr(struct dm_target *ti) {
  struct dmp_target *dt = ti->private;
  dm_put_device(ti, dt->dev);
  kfree(dt);
}

static int dmp_map(struct dm_target *ti, struct bio *bio) {
  struct dmp_target *dt = ti->private;
  unsigned int op = bio_op(bio);
  unsigned int op_flags = bio->bi_opf;

  switch (op) {
  case REQ_OP_READ:
    if (!(op_flags & REQ_RAHEAD)) {
      atomic64_inc(&dmp_stats.count_read);
      atomic64_add(bio->bi_iter.bi_size, &dmp_stats.size_block_read);
    }
    break;
  case REQ_OP_WRITE:
    atomic64_inc(&dmp_stats.count_write);
    atomic64_add(bio->bi_iter.bi_size, &dmp_stats.size_block_write);
    break;
  default:
    return DM_MAPIO_KILL;
  }

  bio_set_dev(bio, dt->dev->bdev);
  return DM_MAPIO_REMAPPED;
}

static struct target_type dmp_target = {
    .features = DM_TARGET_PASSES_INTEGRITY,
    .name = "dmp",
    .module = THIS_MODULE,
    .version = {1, 0, 0},
    .ctr = dmp_ctr,
    .dtr = dmp_dtr,
    .map = dmp_map,
};

static struct kobj_attribute dmp_attr = __ATTR_RO(volumes);

static int __init dmp_init(void) {

  dmp_kobj = kobject_create_and_add("stat", &THIS_MODULE->mkobj.kobj);
  if (!dmp_kobj)
    return -ENOMEM;

  if (sysfs_create_file(dmp_kobj, &dmp_attr.attr)) {
    kobject_put(dmp_kobj);
    return -ENOMEM;
  }

  return dm_register_target(&dmp_target);
}

static void __exit dmp_exit(void) {
  sysfs_remove_file(dmp_kobj, &dmp_attr.attr);
  kobject_put(dmp_kobj);

  dm_unregister_target(&dmp_target);
}

module_init(dmp_init);
module_exit(dmp_exit);

MODULE_AUTHOR("Ilya Kyzkin");
MODULE_DESCRIPTION("Device Mapper Proxy");
MODULE_LICENSE("GPL");
