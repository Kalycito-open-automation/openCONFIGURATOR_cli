/**
 * \file OpenConfiguratorCli.cpp
 *
 * \brief Implementation to receive the command line parameters for
 *        OpenCONFIGURATOR CLI and generate the POWERLINK configuration files
 *        at the output path
 *
 * \author Kalycito Infotech Private Limited
 *
 * \version 1.0
 *
 */
/*------------------------------------------------------------------------------
Copyright (c) 2016, Kalycito Infotech Private Limited, INDIA.
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holders nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
------------------------------------------------------------------------------*/

#include "OpenConfiguratorCli.h"
#include "ParameterValidator.h"
#include "ConfigurationGenerator.h"
#include "ErrorCodeParser.h"

OpenConfiguratorCli::OpenConfiguratorCli() :
	xmlFilePath(""),
	outputPath(""),
	networkName("")
{
}

OpenConfiguratorCli::~OpenConfiguratorCli()
{
}

OpenConfiguratorCli& OpenConfiguratorCli::GetInstance()
{
	static OpenConfiguratorCli instance;

	return instance;
}

CliResult OpenConfiguratorCli::GenerateConfigurationFiles(const std::vector<std::string>& paramsList)
{
	std::ifstream boosLogInifile;
	std::string outStream;
	std::ostringstream logConfString;
	CliResult res;
	bool isFileLoggingEnabled = false;

	/** Check for the help option */
	if (GetHelpOption(paramsList))
	{
		ShowUsage();

		return CliResult(CliErrorCode::USAGE,
				kMsgAppDescription[CliLogger::GetInstance().languageIndex]);
	}

	/** Load the error code table */
	res = ErrorCodeParser::GetInstance().LoadErrorCodeFile();
	if (!res.IsSuccessful())
	{
		LOG_WARN() << CliLogger::GetInstance().GetErrorString(res);
	}

	/** Initialize logging configurations from ini file */
	isFileLoggingEnabled = IsLogDebug(paramsList);

	res = ParameterValidator::GetInstance().IsFileExists(kLogConfigurationFileName);
	if (!res.IsSuccessful())
	{
		/** Boost ini file not exists */
		LOG_WARN() << CliLogger::GetInstance().GetErrorString(res);
	}
	else
	{
		boosLogInifile.open(kLogConfigurationFileName);
		if (boosLogInifile.is_open())
		{
			while (boosLogInifile)
			{
				std::getline(boosLogInifile, outStream);
				logConfString << outStream << std::endl;
				if (isFileLoggingEnabled)
				{
					/** Filter Console target */
					if (outStream.compare("[Sinks.Console]") == 0)
					{
						logConfString << "Filter=\"%Target% contains \\\"Console\\\"\"" << std::endl;
					}
				}
				else
				{
					/** Filter File target */
					if (outStream.compare("[Sinks.File]") == 0)
					{
						logConfString << "Filter=\"%Target% contains \\\"File\\\"\"" << std::endl;
					}
				}
			}
		
			boosLogInifile.close();

			Result confRes = OpenConfiguratorCore::GetInstance().InitLoggingConfiguration(logConfString.str());
			if (!confRes.IsSuccessful())
			{
				LOG_WARN() << CliLogger::GetInstance().GetErrorString(confRes);
			}
		}
	}

	/** Check the parameter option given */
	if (IsParametersValid(paramsList) == false)
	{
		ShowUsage();

		return CliResult(CliErrorCode::INVALID_PARAMETERS,
				kMsgInvalidParameters[CliLogger::GetInstance().languageIndex]);
	}

	if (GetXmlFileName(paramsList))
	{
		if (GetOutputPath(paramsList))
		{
			if (IsLanguageGerman(paramsList))
			{
				/** Initiate CliLogger for German language */
				Result res = OpenConfiguratorCore::GetInstance().SetLoggingLanguage(Language::DE);
				if (res.IsSuccessful())
				{
					CliLogger::GetInstance().languageIndex = (std::uint32_t)Language::DE;
				}
			}

			/** Validate the parameters */
			res = ParameterValidator::GetInstance().IsXmlFileValid(xmlFilePath);
			if (!res.IsSuccessful())
			{
				return res;
			}

			res = ParameterValidator::GetInstance().IsPathValid(outputPath);
			if (!res.IsSuccessful())
			{
				LOG_WARN() << res.GetErrorMessage();

				try
				{
					/** Create the output path as it doesnt exists */
					boost::filesystem::path dir(outputPath);
					if (!boost::filesystem::create_directory(dir))
					{
						/** Failed to create the output path */
						return res;
					}
				}
				catch(const std::exception& e)
				{
					return CliLogger::GetInstance().GetFailureErrorString(e);
				}
			}

			/** Parse and Generate configuration output */
			res = ConfigurationGenerator::GetInstance().GenerateOutputFiles(xmlFilePath, outputPath);
			if (!res.IsSuccessful())
			{
				/** Unable to parse XML or generate configuration files */
				return res;
			}

			return CliResult();
		}
		else
		{
			ShowUsage();

			return CliResult(CliErrorCode::OUTPUT_PATH_NOT_FOUND,
								kMsgOutputPathNotFound[CliLogger::GetInstance().languageIndex]);
		}
	}
	else
	{
		ShowUsage();

		return CliResult(CliErrorCode::XML_FILE_NOT_FOUND,
							kMsgXmlFileNotFound[CliLogger::GetInstance().languageIndex]);
	}
}

void OpenConfiguratorCli::ShowUsage()
{
	const std::string kMsgToolGenerator     = "Kalycito Infotech Private Limited ";
	const std::string kMsgToolVendor        = "B&R Internal" ;
	const std::string kMsgToolUsage         = "Usage: openCONFIGURATOR.exe [options]";
	const std::string kMsgOptions           = "Options: ";
	const std::string kMsgProjectParameter  = " -p,--project <ProjectFile> \t Project XML file. ";
	const std::string kMsgOutputParameter   = " -o,--output <OutputPath> \t Output path for generated files. ";
	const std::string kMsgLanguageParameter = " -de,--german \t\t\t German log messages. Default is English.";
	const std::string kMsgLogMessage        = " -d,--debug \t\t\t Log on file. Default logging is on console";
	const std::string kMsgHelpParameter     = " -h,--help \t\t\t Help. ";

	std::cout << kMsgToolGenerator << std::endl;
	std::cout << kMsgToolVendor << std::endl;
	std::cout << kMsgToolUsage << std::endl;
	std::cout << std::endl;
	std::cout << kMsgOptions << std::endl;
	std::cout << kMsgProjectParameter << std::endl;
	std::cout << kMsgOutputParameter << std::endl;
	std::cout << kMsgLanguageParameter << std::endl;
	std::cout << kMsgLogMessage << std::endl;
	std::cout << kMsgHelpParameter << std::endl;
}

bool OpenConfiguratorCli::GetXmlFileName(const std::vector<std::string>& paramsList)
{
	for (std::uint8_t index = 0; index < paramsList.size(); index++)
	{
		/** Search for project file option */
		if ((paramsList.at(index).compare("-p") == 0)  || (paramsList.at(index).compare("--project") == 0))
		{
			/* Check whether next element exists against number of parameters */
			std::uint8_t increment = 1;
			if ((index + increment) < paramsList.size())
			{
				xmlFilePath = paramsList.at(index + 1);
			}
			else
			{
				return false;
			}

			break;
		}
	}

	/** if options '-p' or '--project' not found, consider the first parameter
	     as the project XML file name */
	if (xmlFilePath.empty() && (paramsList.size() > 0))
	{
		xmlFilePath = paramsList.at(0);
	}

	/** Is options '-p' or '--project' not found in parameter list */
	if (xmlFilePath.empty())
	{
		return false;
	}

	networkName = boost::filesystem::basename(xmlFilePath);

	return true;
}

bool OpenConfiguratorCli::GetOutputPath(const std::vector<std::string>& paramsList)
{
	for (std::uint8_t index = 0; index < paramsList.size(); index++)
	{
		/** Search for output path option */
		if ((paramsList.at(index).compare("-o") == 0)  || (paramsList.at(index).compare("--output") == 0))
		{
			/* Check whether next element exists against number of parameters */
			std::uint8_t increment = 1;
			if ((index + increment) < paramsList.size())
			{
				outputPath = paramsList.at(index + 1);

				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return false;
}

bool OpenConfiguratorCli::IsLanguageGerman(const std::vector<std::string>& paramsList)
{
	for (std::uint8_t index = 0; index < paramsList.size(); index++)
	{
		/** Search for German language option */
		if ((paramsList.at(index).compare("-de") == 0)  || (paramsList.at(index).compare("--german") == 0))
		{
			return true;
		}
	}

	return false;
}

bool OpenConfiguratorCli::IsLogDebug(const std::vector<std::string>& paramsList)
{
	for (std::uint8_t index = 0; index < paramsList.size(); index++)
	{
		/** Search for verbose option */
		if ((paramsList.at(index).compare("-d") == 0)  || (paramsList.at(index).compare("--debug") == 0))
		{
			return true;
		}
	}

	return false;
}

bool OpenConfiguratorCli::GetHelpOption(const std::vector<std::string>& paramsList)
{
	for (std::uint8_t index = 0; index < paramsList.size(); index++)
	{
		/** Search for verbose option */
		if ((paramsList.at(index).compare("-h") == 0)  || (paramsList.at(index).compare("--help") == 0))
		{
			return true;
		}
	}

	return false;
}

bool OpenConfiguratorCli::IsParametersValid(const std::vector<std::string>& paramsList)
{
	bool validParams[maxNumberOfParameters] = {};
	std::uint8_t index;

	if (paramsList.size() > maxNumberOfParameters)
	{
		/** Number of paramaters can not exceed the limit */
		return false;
	}

	/** Initialize the validation flag list */
	for (index = 0; index < maxNumberOfParameters; index++)
	{
		validParams[index] = false;
	}

	for (index = 0; index < paramsList.size(); index++)
	{
		if ((paramsList.at(index).compare("-p") == 0)  || (paramsList.at(index).compare("--project") == 0) ||
			(paramsList.at(index).compare("-o") == 0)  || (paramsList.at(index).compare("--output") == 0))
		{
			validParams[index] = true;

			std::uint8_t increment = 1;
			if ((index + increment) < paramsList.size())
			{
				if (paramsList.at(index + increment).compare(0, 1, "-") == 0)
				{
					/** No project file or output path provided */
					return false;
				}
				else
				{
					validParams[index + increment] = true;
				}
			}
			else
			{
				/** No project file or output path provided */
				return false;
			}
		}
		else if ((paramsList.at(index).compare("-d") == 0)  || (paramsList.at(index).compare("--debug") == 0) ||
				(paramsList.at(index).compare("-de") == 0)  || (paramsList.at(index).compare("--german") == 0))
		{
			validParams[index] = true;
		}
	}

	/** Check the validation flag list */
	for (index = 0; index < paramsList.size(); index++)
	{
		if (validParams[index] == false)
		{
			return false;
		}
	}

	return true;
}
