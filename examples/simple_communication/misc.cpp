// Define behavior of SmartInverter
void SmartInverter::executeWithSkynet(skynet::Job& job, skynet::MasterHandle masterHandle)
{

  // create reduce group for voltage magnitudes
  skynet::ReduceValueTag<double> reduceVoltageMagValue(name + "/voltage_mag_value");
  std::vector<skynet::ReduceValueTag<double>> reduceVoltageMagValues;
  reduceVoltageMagValues.emplace_back(name + "/voltage_mag_value");
  for (auto& peerName : peerNames)
    reduceVoltageMagValues.emplace_back(peerName + "/voltage_mag_value");
  auto fut = job.create_reduce_group(
    skynet::ReduceGroupTag<double>{"voltage_mag_reduce"}, reduceVoltageMagValue,
    reduceVoltageMagValues);
  auto& reduceGroup = fut.get();

  // determine index
  int ind = std::stoi(name.substr(13,1)) - 1;

  double relativeDeviation;
  double time = 0.0;
  std::complex<double> voltage;
  while (time < finalTime)
  {
    if (job.has_data(*subCurtailmentCommand))
    {
      // obtain local voltage
      time = *job.get_waiter(*subCurtailmentCommand).get();
      std::cout << "SmartInverter: Received curtailment command for time " << time << std::endl;
      inverter.getVoltage(time, voltage);
      std:: cout << "SmartInverter: Measured voltage " << voltage << std::endl;
      // impute voltage magnitude using reduction
      const double value = regressionWeights[ind]*std::abs(voltage);
      auto waiter = reduceGroup.allreduce(std::plus<>{}, value);
      const auto result = waiter.get();
      if (!result)
      {
        std::cerr << "SmartInverter: Voltage imputation failed" << std::endl;
        break;
      }
      const double imputedVoltage = *result + regressionIntercept;
      // determine whether to accept or reject curtailment (based on If voltage
      // magnitude deviates beyond 5% of nominal)
      relativeDeviation =
        std::abs(imputedVoltage - NOMINAL_VOLTAGE_MAGNITUDE)/NOMINAL_VOLTAGE_MAGNITUDE;
      std::cout << "SmartInverter: Imputed voltage is " << imputedVoltage
                << ", which is " << relativeDeviation*100 << "% from nominal"
                << std::endl;
      // If voltage magnitude deviates beyond 5% of nominal, send curtailment command
      if (relativeDeviation > 0.05)
        std::cout << "SmartInverter: curtailment command ACCEPTED" << std::endl;
      else
        std::cout << "SmartInverter: curtailment command REJECTED" << std::endl;
    }
    std::this_thread::sleep_for(LOOP_DELAY);
  }
}
