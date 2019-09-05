
double do_dist_dot_product()
{
  double my_sum = ai * bi;
  auto fut = skynet::reduce(my_sum, skynet::plus)

  fut.wait(skynet::ec::throw_on_err{});

  return fut.get();
}

bool do_dist_broadcast()
{
  double my_val;
  skynet::tag tag = skynet::job::tags(27);

  auto fut = skynet::broadcast::send(my_val, tag);

  skynet::ec ec;
  fut.wait(ec);
  // send() syscalls to neighbors have completed at this point, if ec == nil

  return ec != skynet::ec::nil;
}

double get_broadcast_val()
{
  skynet::tag tag = skynet::job::tags(27);
  auto fut = skynet::broadcast::receive<double>(tag);

  using namespace std::chrono_literals;
  skynet::timeout timeout = 5ms;

  fut.wait(skynet::ec::throw_on_err{}, timeout);

  return fut.get();
}
