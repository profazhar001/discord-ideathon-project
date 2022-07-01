#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>
#include <dpp/fmt/format.h>
#include <iomanip>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <chrono>
#include <sstream>
#include <fstream>
#include "csv.h"

const std::string BOT_TOKEN =
    "OTU3MzQ2MjIyNDA5MTUwNTQ2.Yj9cJg.ZEdN91ewnkaQ1t487gtSW2_NUvQ";
const uint64_t GUILD_ID_RAW = 949769553695629376;
const dpp::snowflake GUILD_ID = GUILD_ID_RAW;

// WARNING: blocking function!, does not asynchronously sort messages
void sort_channel_messages(dpp::channel channel, dpp::cluster *bot) {
  // TODO(adrian): We seem to get sync blocked here on messages_get_sync.
  //               Check if Discord bot has necessary permissions from the
  //               invite link used to add it to the server.
  dpp::message_map messages =
      bot->messages_get_sync(957381283388981260, 0, 0, 957432631807602689, 99);

  for (std::pair<dpp::snowflake, dpp::message> message : messages) {
    std::string out;
    out = message.second.author.username + " posted on " + "channel " + channel.name + ": " + message.second.content;
    bot->log(dpp::ll_debug, out);
  }
}

void foreach_channel(dpp::cluster *bot) {

}

void on_channels_get(const dpp::confirmation_callback_t& c) {
  if (!c.is_error()) {
    c.bot->log(dpp::ll_debug, "Succesfully obtained channels list.");
  } else {
    c.bot->log(dpp::ll_error, "Unsuccesfully obtained channels list.");
    return;
  }

  dpp::channel_map map;
  if (std::holds_alternative<dpp::channel_map>(c.value)) {
    try {
      map = std::get<dpp::channel_map>(c.value);
    } catch (std::bad_variant_access&) {
      c.bot->log(dpp::ll_error, "Invalid Variant: Variant does not contain dpp::channel_map");
      return;
    }
  }

  for (std::pair<dpp::snowflake, dpp::channel> channel : map) {
    if (channel.second.is_text_channel()) {
      std::string out = "Channel " + channel.second.name;
      c.bot->log(dpp::ll_debug, out);
    }
  }
}

int main() {
  dpp::cluster bot(BOT_TOKEN, dpp::i_default_intents | dpp::i_message_content);

  const std::string log_name = "IDEATHON_BOT_LOG.txt";

  /* Set up spdlog logger */
  std::shared_ptr<spdlog::logger> log;
  spdlog::init_thread_pool(8192, 2);
  std::vector<spdlog::sink_ptr> sinks;
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      log_name, 1024 * 1024 * 5, 10);
  sinks.push_back(stdout_sink);
  sinks.push_back(rotating);
  log = std::make_shared<spdlog::async_logger>(
      "logs", sinks.begin(), sinks.end(), spdlog::thread_pool(),
      spdlog::async_overflow_policy::block);
  spdlog::register_logger(log);
  log->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");
  log->set_level(spdlog::level::level_enum::debug);
  spdlog::flush_every(std::chrono::seconds(2));

  bot.on_interaction_create([&log](const dpp::interaction_create_t &event) {
    if (event.command.get_command_name() == "ping") {
      event.reply("Pong!");
      log->debug("{}", "Pong!");
    }
  });

  dpp::command_completion_event_t CALLBACK_foreach_channel;

  CALLBACK_foreach_channel = [](const dpp::confirmation_callback_t &c) {
    if (!c.is_error()) {
      c.bot->log(dpp::ll_debug, "Successfully obtained messages");
    } else {
      c.bot->log(dpp::ll_error, "Unsuccessfully obtained messages");
      return;
    }

    dpp::message_map map;
    if (std::holds_alternative<dpp::message_map>(c.value)) {
      try {
        map = std::get<dpp::message_map>(c.value);
      } catch (std::bad_variant_access&){
        c.bot->log(dpp::ll_error, "Invalid Variant: Variant does not contain dpp::message_map");
        return;
      }
    }

    for (std::pair<dpp::snowflake, dpp::message> message : map) {
      // TODO(adrian): time/date is hard. This method works for now,
      //               but look into using c++20 chrono features
      //               for a more elegant way to converting
      //               seconds since epoch into a human readable format
      time_t message_timestamp = message.second.sent;
      std::tm tm = *std::gmtime(&message_timestamp);
      std::stringstream ss_out;
      ss_out << std::put_time(&tm, "%c");


      std::string out;
      out = message.second.author.username + " posted: " + message.second.content;
      c.bot->log(dpp::ll_debug, ss_out.str());
      c.bot->log(dpp::ll_debug, out);
    }
  };

  dpp::command_completion_event_t CALLBACK_channels;
  //CALLBACK_channels = on_channels_get;

  //auto channels =
  //    std::make_unique<std::unordered_map<dpp::snowflake, dpp::channel>>();

  CALLBACK_channels = [&bot, &CALLBACK_foreach_channel](const dpp::confirmation_callback_t &c) {
    if (!c.is_error()) {
      bot.log(dpp::ll_debug, "Succesfully obtained channels list.");
    } else {
      bot.log(dpp::ll_error, "Unsuccesfully obtained channels list.");
      return;
    }

    dpp::channel_map map;
    if (std::holds_alternative<dpp::channel_map>(c.value)) {
      try {
        map = std::get<dpp::channel_map>(c.value);
      } catch (std::bad_variant_access &) {
        c.bot->log(
            dpp::ll_error,
            "Invalid Variant: Variant does not contain dpp::channel_map");
        return;
      }
    }

    for (std::pair<dpp::snowflake, dpp::channel> channel : map) {
      if (channel.second.is_text_channel()) {
        std::string out = "Channel " + channel.second.name;
        c.bot->log(dpp::ll_debug, out);

        // TODO(adrian): This is the synchronous method for obtaining messages.
        //               There is a weird hang on the thread
        //sort_channel_messages(channel.second, &bot);

        // TODO(adrian): This is the asynchronous method for obtaining messages
        bot.messages_get(channel.second.id, 0, 0, 0, 100, [channel](const dpp::confirmation_callback_t &c) {
          if (!c.is_error()) {
            c.bot->log(dpp::ll_debug, "Successfully obtained messages");
          } else {
            c.bot->log(dpp::ll_error, "Unsuccessfully obtained messages");
            return;
          }

          dpp::message_map msg_map;
          if (std::holds_alternative<dpp::message_map>(c.value)) {
            try {
              msg_map = std::get<dpp::message_map>(c.value);
            } catch (std::bad_variant_access&){
              c.bot->log(dpp::ll_error, "Invalid Variant: Variant does not contain dpp::message_map");
              return;
            }
          }

          std::ofstream fout(std::string("SOC111-CSC211H") + "_" + channel.second.name +
              ".txt", std::fstream::out | std::fstream::trunc);
          auto writer = csv::make_csv_writer(fout);
          writer << std::make_tuple("timestamp", "channel", "discord_username", "server_nickname", "message");

          for (std::pair<dpp::snowflake, dpp::message> message : msg_map) {
            // TODO(adrian): time/date is hard. This method works for now,
            //               but look into using c++20 chrono features
            //               for a more elegant way to converting
            //               seconds since epoch into a human readable format
            time_t message_timestamp = message.second.sent;
            std::tm tm = *std::gmtime(&message_timestamp);
            std::stringstream ss_out;
            ss_out << std::put_time(&tm, "%c");


            std::string out;
            out = message.second.author.username + " posted on " + channel.second.name + ": " + message.second.content;
            c.bot->log(dpp::ll_debug, ss_out.str());
            c.bot->log(dpp::ll_debug, out);

            writer << std::make_tuple(ss_out.str(),
                channel.second.name,
                message.second.author.username,
                message.second.member.nickname,
                message.second.content
                );
          }
        }
          );
      }
    }
  };

  /* Integrate spdlog logger to D++ log events */
  bot.on_log([&bot, &log](const dpp::log_t &event) {
    switch (event.severity) {
    case dpp::ll_trace:
      log->trace("{}", event.message);
      break;
    case dpp::ll_debug:
      log->debug("{}", event.message);
      break;
    case dpp::ll_info:
      log->info("{}", event.message);
      break;
    case dpp::ll_warning:
      log->warn("{}", event.message);
      break;
    case dpp::ll_error:
      log->error("{}", event.message);
      break;
    case dpp::ll_critical:
    default:
      log->critical("{}", event.message);
      break;
    }
  });



  bot.on_ready([&bot, &CALLBACK_channels](const dpp::ready_t &event) {
    if (dpp::run_once<struct register_bot_commands>()) {
      //bot.guild_command_create(
          //dpp::slashcommand("ping", "Ping pong!", bot.me.id), GUILD_ID);

    }
    bot.channels_get(GUILD_ID, CALLBACK_channels);
  });

  bot.start(false);
  spdlog::shutdown();
  return 0;
}
