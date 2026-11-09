-- ProgressionMgr commands (.progression). Given to administrators (role 196).
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 2011 AND 2014;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(2011, 'Command: progression'),
(2012, 'Command: progression info'),
(2013, 'Command: progression set'),
(2014, 'Command: progression reload');

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 2011 AND 2014;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(196, 2011),
(196, 2012),
(196, 2013),
(196, 2014);
