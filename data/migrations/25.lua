function onUpdateDatabase()
	print("> Updating database to version 25 (Binary Save)")
	db.query("ALTER TABLE `players` ADD `spells` blob DEFAULT NULL")
	db.query("ALTER TABLE `players` ADD `items` longblob DEFAULT NULL")
	db.query("ALTER TABLE `players` ADD `depotitems` longblob DEFAULT NULL")
	db.query("ALTER TABLE `players` ADD `inboxitems` longblob DEFAULT NULL")
	db.query("DROP TABLE `player_spells`")
	db.query("DROP TABLE `player_items`")
	db.query("DROP TABLE `player_depotitems`")
	db.query("DROP TABLE `player_inboxitems`")
	return true
end
