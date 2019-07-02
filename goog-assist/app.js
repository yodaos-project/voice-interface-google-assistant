var Application = require('@yodaos/application').Application
var _ = require('@yoda/util')._
var logger = require('logger')('app')

module.exports = Application({
  url: function Url (url) {
    logger.info('url', url)
    var request
    try {
      request = JSON.parse(url.query.request)
    } catch (err) {}
    var command = _.get(request, 'inputs.0.payload.commands.0.execution.0.command')
    logger.info('command', command)
    switch (command) {
      case 'com.example.commands.Open-Bluetooth':
        this.openUrl('yoda-app://bluetooth/open_and_play').catch(err => { logger.error('err', err) })
        break
      case 'com.example.commands.Close-Bluetooth':
        this.openUrl('yoda-app://bluetooth-music/stop').catch(err => { logger.error('err', err) })
        break
    }
  }
})
